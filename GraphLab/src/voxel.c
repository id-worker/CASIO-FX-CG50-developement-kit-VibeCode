/* Voxel rendering, no surface triangle/polygon mesh. Consecutive xy sample
   planes form an implicit shell; inequalities classify cell centers as a
   bounded volume. Exposed voxel faces are projected as raster scanlines,
   depth tested per pixel, and the caller controls when to dupdate(). */
#include "voxel.h"
#include "camera_near.h"
#include "platform.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>


typedef struct {float x,y,z;} Vec;
typedef struct {int x,y,z;} Screen;
typedef struct {
    Program const *program;EvalVars vars;int implicit;VoxelStats *stats;
} Field;
typedef struct {
    Volume v;Camera c;int n;size_t bytes;unsigned char *bits[2];uint16_t *depth;
    float cy,sy,cp,sp,ex,ey,ez,near_clip,inv_near,depth_scale;
    float clip_left,clip_right,clip_bottom,clip_top;
    double grid_step_exact,eye_x_exact,eye_y_exact,eye_z_exact;
    int slots[2],view_inside;VoxelStats *stats;
} Renderer;
static void setbit(unsigned char *a,size_t i){a[i>>3]|=(unsigned char)(1u<<(i&7));}
static int bit(unsigned char const *a,size_t i){return !!(a[i>>3]&(1u<<(i&7)));}
static size_t index3(int x,int y,int z,int n){return ((size_t)z*n+y)*n+x;}
static int occupied(Renderer const *r,int x,int y,int z)
{
    if(x<0||y<0||z<0||x>=r->n||y>=r->n||z>=r->n)return 0;
    size_t i=index3(x,y,z,r->n);
    if(r->bits[0]&&bit(r->bits[0],i))return 1;
    if(r->bits[1]&&bit(r->bits[1],i))return 2;
    return 0;
}
static float field_at(Field *f,float x,float y,float z)
{
    f->vars.v[V_X]=x;f->vars.v[V_Y]=y;f->vars.v[V_Z]=z;
    f->stats->evaluations++;
    float q=expr_eval_real(f->program,&f->vars);
    if(!isfinite(q)){f->stats->invalid++;return NAN;}
    return f->implicit?q:z-q;
}
static int x_period(Program const *p,EvalVars const *base,float period,float step,int n)
{
    if(!expr_program_uses(p,V_X))return 1;
    if(!isfinite(period)||period<=0||!isfinite(step)||step<=0)return 0;
    float k=period/step;
    if(!isfinite(k)||k<1||k>n||!expr_proves_x_period(p,base,period))return 0;
    int result=(int)lroundf(k);
    return fabsf(k-result)<=.000001f?result:0;
}
/* Probe an opposing-sign edge to reject many poles. This is a heuristic, not
   an interval proof, so thin features and discontinuities may still be lost. */
static int plausible_edge(Field *f,Vec a,Vec b,float va,float vb)
{
    float small=fminf(fabsf(va),fabsf(vb)),scale=fmaxf(fabsf(va),fabsf(vb));
    if(!isfinite(scale)||scale<=0)return 0;
    float best=INFINITY;
    for(int j=0;j<3;j++){
        float den=fabsf(va)+fabsf(vb);
        float t=(j==0 && isfinite(den)&&den>0)?fabsf(va)/den:.5f;
        Vec p={a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t};
        float q=field_at(f,p.x,p.y,p.z);
        if(!isfinite(q))return 0;
        if(fabsf(q)<best)best=fabsf(q);
        if(q==0||best<=small*.45f+scale*.0001f)return 1;
        if((q<0)==(va<0)){a=p;va=q;}else{b=p;vb=q;}
    }
    return best<=small*.45f+scale*.0001f;
}
static int build_surface(Renderer *r,Field *f,int si,ProgressFn progress,void *ctx)
{
    int n=r->n,d=n+1;size_t plane=(size_t)d*d;
    float *memory=malloc(2*plane*sizeof(float));if(!memory)return -2;
    float *prev=memory,*next=memory+plane,step=2*r->v.radius/n;
    int period=x_period(f->program,&f->vars,r->v.period_x,step,n);
    /* A finite sign-changing edge of a proven continuous real expression
       must contain a root. The pole heuristic used to reject real surface
       cells in smooth trigonometric functions (including sin(x)+sin(y)+sin(z)). */
    int continuous=expr_program_continuous(f->program);
    static const int corner[8][3]={{0,0,0},{1,0,0},{0,1,0},{1,1,0},
                                   {0,0,1},{1,0,1},{0,1,1},{1,1,1}};
    static const int edges[12][2]={{0,1},{2,3},{4,5},{6,7},{0,2},{1,3},
                                   {4,6},{5,7},{0,4},{1,5},{2,6},{3,7}};
    for(int z=0;z<=n;z++){
        if(progress&&!progress(z,n+1,ctx)){free(memory);return 0;}
        float wz=r->v.z-r->v.radius+step*z;
        for(int y=0;y<=n;y++){
            if((y&7)==0 && progress&&!progress(z,n+1,ctx)){free(memory);return 0;}
            float wy=r->v.y-r->v.radius+step*y;
            for(int x=0;x<=n;x++){
                size_t a=(size_t)y*d+x;float q;
                if(period&&x>=period)q=next[a-period];
                else if(z&&!f->implicit&&isfinite(prev[a]))q=prev[a]+step;
                else q=field_at(f,r->v.x-r->v.radius+step*x,wy,wz);
                next[a]=q;r->stats->samples++;
            }
        }
        if(z)for(int y=0;y<n;y++){
            if((y&7)==0 && progress&&!progress(z,n+1,ctx)){free(memory);return 0;}
            for(int x=0;x<n;x++){
                size_t a=(size_t)y*d+x;
                float q[8]={prev[a],prev[a+1],prev[a+d],prev[a+d+1],
                            next[a],next[a+1],next[a+d],next[a+d+1]};
                int neg=0,pos=0,zero=0,valid=1;
                for(int k=0;k<8;k++){
                    if(!isfinite(q[k])){valid=0;break;}
                    if(q[k]<0)neg=1;else if(q[k]>0)pos=1;else zero=1;
                }
                r->stats->cells++;
                if(!valid||(!zero&&!(neg&&pos)))continue;
                if(!zero&&!continuous){
                    int accepted=0;
                    for(int j=0;j<12;j++){
                        int a0=edges[j][0],b0=edges[j][1];
                        if((q[a0]<0)==(q[b0]<0))continue;
                        Vec va={r->v.x-r->v.radius+step*(x+corner[a0][0]),
                                r->v.y-r->v.radius+step*(y+corner[a0][1]),
                                r->v.z-r->v.radius+step*(z-1+corner[a0][2])};
                        Vec vb={r->v.x-r->v.radius+step*(x+corner[b0][0]),
                                r->v.y-r->v.radius+step*(y+corner[b0][1]),
                                r->v.z-r->v.radius+step*(z-1+corner[b0][2])};
                        accepted=plausible_edge(f,va,vb,q[a0],q[b0]);break;
                    }
                    if(!accepted){r->stats->rejected++;continue;}
                }
                setbit(r->bits[si],index3(x,y,z-1,n));r->stats->occupied++;
            }
        }
        float *swap=prev;prev=next;next=swap;
    }
    free(memory);return 1;
}
static int build_volume(Renderer *r,Field *f,int si,ProgressFn progress,void *ctx)
{
    int n=r->n;float step=2*r->v.radius/n;
    int period=x_period(f->program,&f->vars,r->v.period_x,step,n);
    float *row=period?malloc((size_t)n*sizeof(float)):NULL;
    if(period&&!row)period=0;
    for(int z=0;z<n;z++){
        if(progress&&!progress(z,n,ctx)){free(row);return 0;}
        float wz=r->v.z-r->v.radius+step*(z+.5f);
        for(int y=0;y<n;y++){
            if((y&7)==0 && progress&&!progress(z,n,ctx)){free(row);return 0;}
            float wy=r->v.y-r->v.radius+step*(y+.5f);
            for(int x=0;x<n;x++){
                float q;
                if(period&&x>=period)q=row[x-period];
                else q=field_at(f,r->v.x-r->v.radius+step*(x+.5f),wy,wz);
                if(period)row[x]=q;
                r->stats->samples++;r->stats->cells++;
                if(expr_relation_true((Relation)f->program->relation,q)){
                    setbit(r->bits[si],index3(x,y,z,n));r->stats->occupied++;
                }
            }
        }
    }
    free(row);return 1;
}
static int prepare(Renderer *r,Volume const *v,Camera const *c,VoxelStats *stats)
{
    if(!v||!c||v->div<VOXEL_MIN_DIV||v->div>VOXEL_MAX_DIV||
       !isfinite(v->radius)||v->radius<=0||!isfinite(v->x)||!isfinite(v->y)||!isfinite(v->z)||
       !isfinite(c->yaw)||!isfinite(c->pitch)||!isfinite(c->zoom)||c->zoom<.1f||c->zoom>5||
       !isfinite(c->eye_x)||!isfinite(c->eye_y)||!isfinite(c->eye_z))return 0;
    memset(r,0,sizeof *r);r->v=*v;r->c=*c;r->n=v->div;r->stats=stats;
    r->cy=cosf(c->yaw);r->sy=sinf(c->yaw);r->cp=cosf(c->pitch);r->sp=sinf(c->pitch);
    r->ex=(c->eye_x-v->x)/v->radius;r->ey=(c->eye_y-v->y)/v->radius;r->ez=(c->eye_z-v->z)/v->radius;
    float center_depth=r->cp*(r->sy*r->ex+r->cy*r->ey)+r->sp*r->ez;
    if(!isfinite(center_depth)||!isfinite(r->ex)||!isfinite(r->ey)||!isfinite(r->ez))return 0;
    r->near_clip=camera_near_normalized(v->radius);
    r->grid_step_exact=2.0/(double)r->n;
    r->eye_x_exact=((double)v->x-(double)c->eye_x)/(double)v->radius;
    r->eye_y_exact=((double)v->y-(double)c->eye_y)/(double)v->radius;
    r->eye_z_exact=((double)v->z-(double)c->eye_z)/(double)v->radius;
    float focal=72.0f*2.8f*c->zoom;
    float sx=(PLOT_X0+PLOT_X1)*.5f,sy=(PLOT_Y0+PLOT_Y1)*.5f;
    r->clip_left=(PLOT_X0-sx)/focal;r->clip_right=(PLOT_X1-sx)/focal;
    r->clip_bottom=(sy-PLOT_Y1)/focal;r->clip_top=(sy-PLOT_Y0)/focal;
    /* Keep 16-bit reciprocal-depth precision useful across a huge cube.
       Very near faces may share the nearest depth bucket, but geometric
       clipping itself is independent of this mapping and remains tiny. */
    float depth_floor=fmaxf(r->near_clip,fminf(.04f,(2.0f/r->n)/32.0f));
    float near=fmaxf(depth_floor,center_depth-2),far=fmaxf(near+.001f,center_depth+2);
    r->inv_near=1.0f/near;float span=r->inv_near-1.0f/far;
    if(!isfinite(span)||span<=0)return 0;
    r->depth_scale=65532.0f/span;
    return isfinite(r->depth_scale);
}
static Vec to_eye(Renderer const *r,Vec p)
{
    p.x-=r->ex;p.y-=r->ey;p.z-=r->ez;
    float ry=r->sy*p.x+r->cy*p.y;
    return (Vec){r->cy*p.x-r->sy*p.y,r->cp*p.z-r->sp*ry,-r->cp*ry-r->sp*p.z};
}
typedef struct {double x,y,z;} DVec;
static DVec to_eye_precise(Renderer const *r,int ix,int iy,int iz)
{
    /* Compute grid coordinates and camera difference before rounding to
       float. A large R can otherwise swallow a tiny camera-to-face gap. */
    double x=-1+r->grid_step_exact*ix+r->eye_x_exact;
    double y=-1+r->grid_step_exact*iy+r->eye_y_exact;
    double z=-1+r->grid_step_exact*iz+r->eye_z_exact;
    double ry=(double)r->sy*x+(double)r->cy*y;
    return (DVec){(double)r->cy*x-(double)r->sy*y,
                  (double)r->cp*z-(double)r->sp*ry,
                  -(double)r->cp*ry-(double)r->sp*z};
}
static float face_plane(Renderer const *r,Vec p,int plane)
{
    switch(plane){
        case 0:return p.z-r->near_clip;
        case 1:return p.x-r->clip_left*p.z;
        case 2:return r->clip_right*p.z-p.x;
        case 3:return p.y-r->clip_bottom*p.z;
        default:return r->clip_top*p.z-p.y;
    }
}
/* Clip the whole convex face, not X/Y independently after projection. The
   result is still one voxel face, with up to nine vertices; no mesh is made. */
static int clip_face(Renderer const *r,Vec const *in,int n,Vec *out)
{
    Vec a[12],b[12];memcpy(a,in,(size_t)n*sizeof *a);
    for(int plane=0;plane<5;plane++){
        if(n<3)return 0;
        int count=0;Vec prev=a[n-1];float gp=face_plane(r,prev,plane);
        for(int i=0;i<n;i++){
            Vec next=a[i];float gn=face_plane(r,next,plane);
            if(!isfinite(gp)||!isfinite(gn))return 0;
            if((gp<0)!=(gn<0)){
                float t=gp/(gp-gn);
                if(!isfinite(t)||count>=11)return 0;
                Vec p={prev.x+(next.x-prev.x)*t,prev.y+(next.y-prev.y)*t,prev.z+(next.z-prev.z)*t};
                /* Snap to the clipping plane, not just its float
                   interpolation. This keeps a very close face from opening
                   large gaps when its original corners are far off-screen. */
                if(plane==0)p.z=r->near_clip;
                else if(plane==1)p.x=r->clip_left*p.z;
                else if(plane==2)p.x=r->clip_right*p.z;
                else if(plane==3)p.y=r->clip_bottom*p.z;
                else p.y=r->clip_top*p.z;
                b[count++]=p;
            }
            if(gn>=0){if(count>=11)return 0;b[count++]=next;}
            prev=next;gp=gn;
        }
        n=count;memcpy(a,b,(size_t)n*sizeof *a);
    }
    memcpy(out,a,(size_t)n*sizeof *out);return n;
}
/* When a face crosses the very small near plane, interpolating large float
   endpoints can lose the whole viewport-sized intersection. The uncommon
   near-face path does its intersections in double; normal faces stay float. */
static double face_plane_precise(Renderer const *r,DVec p,int plane)
{
    switch(plane){
        case 0:return p.z-r->near_clip;
        case 1:return p.x-(double)r->clip_left*p.z;
        case 2:return (double)r->clip_right*p.z-p.x;
        case 3:return p.y-(double)r->clip_bottom*p.z;
        default:return (double)r->clip_top*p.z-p.y;
    }
}
static int clip_face_precise(Renderer const *r,DVec const *in,Vec *out)
{
    DVec a[12],b[12];int n=4;memcpy(a,in,4*sizeof *a);
    for(int plane=0;plane<5;plane++){
        if(n<3)return 0;
        int count=0;DVec prev=a[n-1];double gp=face_plane_precise(r,prev,plane);
        for(int i=0;i<n;i++){
            DVec next=a[i];double gn=face_plane_precise(r,next,plane);
            if(!isfinite(gp)||!isfinite(gn))return 0;
            if((gp<0)!=(gn<0)){
                double t=gp/(gp-gn);
                if(!isfinite(t)||count>=11)return 0;
                DVec p={prev.x+(next.x-prev.x)*t,prev.y+(next.y-prev.y)*t,prev.z+(next.z-prev.z)*t};
                if(plane==0)p.z=r->near_clip;
                else if(plane==1)p.x=(double)r->clip_left*p.z;
                else if(plane==2)p.x=(double)r->clip_right*p.z;
                else if(plane==3)p.y=(double)r->clip_bottom*p.z;
                else p.y=(double)r->clip_top*p.z;
                b[count++]=p;
            }
            if(gn>=0){if(count>=11)return 0;b[count++]=next;}
            prev=next;gp=gn;
        }
        n=count;memcpy(a,b,(size_t)n*sizeof *a);
    }
    for(int i=0;i<n;i++)out[i]=(Vec){(float)a[i].x,(float)a[i].y,(float)a[i].z};
    return n;
}
static int project(Renderer const *r,Vec p,Screen *q)
{
    if(!isfinite(p.x)||!isfinite(p.y)||!isfinite(p.z)||p.z<r->near_clip*.9999f)return 0;
    if(p.z<r->near_clip)p.z=r->near_clip;
    float factor=(72.0f*2.8f)*r->c.zoom/p.z;
    float x=(PLOT_X0+PLOT_X1)*.5f+factor*p.x;
    float y=(PLOT_Y0+PLOT_Y1)*.5f-factor*p.y;
    float z=(r->inv_near-1.0f/p.z)*r->depth_scale+1;
    if(!isfinite(x)||!isfinite(y)||!isfinite(z))return 0;
    /* The polygon was clipped against the full viewport above. Only clamp
       sub-pixel rounding noise, never independently truncate a long edge. */
    if(x<PLOT_X0-1 || x>PLOT_X1+1 || y<PLOT_Y0-1 || y>PLOT_Y1+1)return 0;
    if(x<PLOT_X0)x=PLOT_X0;
    if(x>PLOT_X1)x=PLOT_X1;
    if(y<PLOT_Y0)y=PLOT_Y0;
    if(y>PLOT_Y1)y=PLOT_Y1;
    if(z<1)z=1;
    if(z>65534)z=65534;
    *q=(Screen){(int)lroundf(x),(int)lroundf(y),(int)lroundf(z)};return 1;
}
static uint16_t face_color(int slot,int axis)
{
    int b=axis==2?29:axis==0?23:17;
    if(slot%3==0)return (uint16_t)C_RGB(b/5,b*4/5,b);
    if(slot%3==1)return (uint16_t)C_RGB(b,b/3,b/4);
    return (uint16_t)C_RGB(b/3,b,b/3);
}
typedef struct {int y0,y1;int64_t x,dx;} Edge;
/* Fill horizontal pixel spans of a single convex voxel face. Depth is affine
   in reciprocal perspective depth on a plane; no triangles are generated. */
static void face_pixels(Renderer *r,Screen const *p,int count,int slot,int axis)
{
    int i1=-1,i2=-1,best=0;
    for(int i=1;i<count;i++)for(int j=i+1;j<count;j++){
        int area=(p[i].x-p[0].x)*(p[j].y-p[0].y)-(p[j].x-p[0].x)*(p[i].y-p[0].y);
        int mag=area<0?-area:area;
        if(mag>best){best=mag;i1=i;i2=j;}
    }
    if(best==0)return;
    Screen a=p[0],b=p[i1],c=p[i2];
    int area=(b.x-a.x)*(c.y-a.y)-(c.x-a.x)*(b.y-a.y);
    int64_t nx=(int64_t)(b.z-a.z)*(c.y-a.y)-(int64_t)(c.z-a.z)*(b.y-a.y);
    int64_t ny=(int64_t)(b.x-a.x)*(c.z-a.z)-(int64_t)(c.x-a.x)*(b.z-a.z);
    int64_t dzx=nx*256/area,dzy=ny*256/area;
    Edge e[12];int ne=0,ymin=PLOT_Y1+1,ymax=PLOT_Y0-1;
    for(int i=0;i<count;i++){
        Screen u=p[i],v=p[(i+1)%count];if(u.y==v.y)continue;
        if(u.y>v.y){Screen t=u;u=v;v=t;}
        int y0=u.y,y1=v.y-1;if(y0<PLOT_Y0)y0=PLOT_Y0;if(y1>PLOT_Y1)y1=PLOT_Y1;
        if(y0>y1)continue;
        int64_t slope=(int64_t)(v.x-u.x)*65536/(v.y-u.y);
        int64_t x=(int64_t)u.x*65536+slope*(y0-u.y)+slope/2;
        e[ne++]=(Edge){y0,y1,x,slope};
        if(y0<ymin)ymin=y0;
        if(y1>ymax)ymax=y1;
    }
    if(ne<2||ymin>ymax)return;
    uint16_t col=face_color(slot,axis);
    for(int y=ymin;y<=ymax;y++){
        int64_t lo=INT64_MAX,hi=INT64_MIN;int have=0;
        for(int j=0;j<ne;j++)if(y>=e[j].y0&&y<=e[j].y1){
            if(e[j].x<lo)lo=e[j].x;
            if(e[j].x>hi)hi=e[j].x;
            e[j].x+=e[j].dx;have++;
        }
        if(have<2)continue;
        int64_t left=(int64_t)PLOT_X0*65536+32768,right=(int64_t)PLOT_X1*65536+32768;
        if(lo<left)lo=left;
        if(hi>right)hi=right;
        if(lo>hi)continue;
        int x0=(int)((lo-32768+65535)/65536),x1=(int)((hi-32768)/65536);
        if(x0>x1)continue;
        int64_t depth=(int64_t)a.z*256+dzx*(x0-a.x)+dzy*(y-a.y);
        for(int x=x0;x<=x1;x++,depth+=dzx){
            int64_t z=depth/256;if(z<1)z=1;
            if(z>65534)z=65534;
            size_t index=(size_t)(y-PLOT_Y0)*PLOT_W+(x-PLOT_X0);
            if(z<=r->depth[index]){
                r->depth[index]=(uint16_t)z;
                /* Flat shaded voxel faces, without a dark per-face outline. */
                gint_vram[(size_t)y*DWIDTH+x]=col;r->stats->pixels++;
            }
        }
    }
    r->stats->faces++;
}
static void draw_voxel(Renderer *r,int x,int y,int z,int surface)
{
    static const int vertices[6][4]={{0,4,6,2},{1,3,7,5},{0,1,5,4},
                                     {2,6,7,3},{0,2,3,1},{4,5,7,6}};
    static const int adjacent[6][3]={{-1,0,0},{1,0,0},{0,-1,0},{0,1,0},{0,0,-1},{0,0,1}};
    float step=2.0f/r->n,x0=-1+step*x,y0=-1+step*y,z0=-1+step*z;
    for(int face=0;face<6;face++){
        if(occupied(r,x+adjacent[face][0],y+adjacent[face][1],z+adjacent[face][2]))continue;
        if(!r->view_inside && ((face==0&&r->ex>=x0)||(face==1&&r->ex<=x0+step)||
           (face==2&&r->ey>=y0)||(face==3&&r->ey<=y0+step)||
           (face==4&&r->ez>=z0)||(face==5&&r->ez<=z0+step)))continue;
        Vec corners[4],in[4],out[12];float nearz=INFINITY,farz=-INFINITY;
        for(int k=0;k<4;k++){
            int v=vertices[face][k];
            /* The same integer grid point must produce the same float on
               both adjacent voxels, including at the positive face. */
            corners[k]=(Vec){-1+step*(x+!!(v&1)),-1+step*(y+!!(v&2)),-1+step*(z+!!(v&4))};
            in[k]=to_eye(r,corners[k]);
            nearz=fminf(nearz,in[k].z);farz=fmaxf(farz,in[k].z);
        }
        if(farz<-.0001f)continue;
        int n;
        if(nearz<.0001f){
            DVec precise[4];
            for(int k=0;k<4;k++){
                int v=vertices[face][k];
                precise[k]=to_eye_precise(r,x+!!(v&1),y+!!(v&2),z+!!(v&4));
            }
            n=clip_face_precise(r,precise,out);
        }else n=clip_face(r,in,4,out);
        if(n<3)continue;
        Screen screen[12];int good=1;
        for(int k=0;k<n;k++)if(!project(r,out[k],&screen[k])){good=0;break;}
        if(good)face_pixels(r,screen,n,r->slots[surface-1],face/2);
    }
}
/* Recover only visible points from the existing reciprocal depth buffer.
   No formula evaluation and no second geometry representation is needed.
   Saturated depth buckets cannot be reliably inverted and are skipped. */
static void capture_preview(Renderer const *r,Preview3D *preview)
{
    if(!preview||!preview->point||!r->depth)return;
    float focal=72.0f*2.8f*r->c.zoom;
    for(int y=PLOT_Y0;y<=PLOT_Y1;y+=PREVIEW3D_STEP)for(int x=PLOT_X0;x<=PLOT_X1;x+=PREVIEW3D_STEP){
        size_t at=(size_t)(y-PLOT_Y0)*PLOT_W+(x-PLOT_X0);uint16_t bucket=r->depth[at];
        if(bucket<=1||bucket>=65534)continue;
        float inverse=r->inv_near-(bucket-1)/r->depth_scale;
        if(!isfinite(inverse)||inverse<=0)continue;
        float z=1/inverse,ex=(x-(PLOT_X0+PLOT_X1)*.5f)*z/focal;
        float ey=((PLOT_Y0+PLOT_Y1)*.5f-y)*z/focal;
        float ry=-r->sp*ey-r->cp*z,dz=r->cp*ey-r->sp*z;
        float dx=r->cy*ex+r->sy*ry,dy=-r->sy*ex+r->cy*ry;
        float wx=r->v.x+r->v.radius*(r->ex+dx);
        float wy=r->v.y+r->v.radius*(r->ey+dy);
        float wz=r->v.z+r->v.radius*(r->ez+dz);
        preview3d_add(preview,x,y,wx,wy,wz,gint_vram[(size_t)y*DWIDTH+x]);
    }
}
int voxel3d_render_ex(RaySurface const *surfaces,int count,EvalVars const *base,
                   Volume const *volume,Camera const *camera,Preview3D *preview,
                   ProgressFn progress,void *ctx,VoxelStats *stats)
{
    if(!surfaces||count<1||count>RAY_MAX_SURFACES||!base||!stats)return -1;
    memset(stats,0,sizeof *stats);Renderer r;
    if(!prepare(&r,volume,camera,stats))return -1;
    size_t cells=(size_t)r.n*r.n*r.n;r.bytes=(cells+7)/8;
    int result=1;
    for(int i=0;i<count;i++){
        if(!surfaces[i].program||surfaces[i].slot<0||surfaces[i].slot>2){result=-1;goto done;}
        r.slots[i]=surfaces[i].slot;r.bits[i]=calloc(r.bytes,1);
        if(!r.bits[i]){result=-2;goto done;}
        Field f={surfaces[i].program,*base,
                 surfaces[i].program->has_equal||expr_program_uses(surfaces[i].program,V_Z),stats};
        if(surfaces[i].program->relation>=REL_LT)result=build_volume(&r,&f,i,progress,ctx);
        else result=build_surface(&r,&f,i,progress,ctx);
        if(result!=1)goto done;
    }
    /* When the observer is inside an occupied voxel, include back-facing
       boundary faces so a filled volume is still visible from within. */
    float ox=(r.ex+1)*.5f*r.n,oy=(r.ey+1)*.5f*r.n,oz=(r.ez+1)*.5f*r.n;
    if(isfinite(ox)&&isfinite(oy)&&isfinite(oz)&&ox>=0&&oy>=0&&oz>=0&&ox<r.n&&oy<r.n&&oz<r.n)
        r.view_inside=occupied(&r,(int)ox,(int)oy,(int)oz)!=0;
    r.depth=malloc((size_t)PLOT_W*PLOT_H*sizeof(uint16_t));
    if(!r.depth){result=-2;goto done;}
    for(int i=0;i<PLOT_W*PLOT_H;i++)r.depth[i]=65535;
    for(int z=0;z<r.n;z++){
        if(progress&&!progress(z,r.n,ctx)){result=0;goto done;}
        for(int y=0;y<r.n;y++){
            if((y&7)==0 && progress&&!progress(z,r.n,ctx)){result=0;goto done;}
            for(int x=0;x<r.n;x++){
                int s=occupied(&r,x,y,z);if(s)draw_voxel(&r,x,y,z,s);
            }
        }
    }
    if(progress&&!progress(r.n,r.n,ctx))result=0;
    if(result==1)capture_preview(&r,preview);
done:
    if(result==0)stats->canceled=1;
    free(r.depth);free(r.bits[0]);free(r.bits[1]);return result;
}

int voxel3d_render(RaySurface const *surfaces,int count,EvalVars const *base,
                   Volume const *volume,Camera const *camera,
                   ProgressFn progress,void *ctx,VoxelStats *stats)
{
    return voxel3d_render_ex(surfaces,count,base,volume,camera,NULL,progress,ctx,stats);
}
