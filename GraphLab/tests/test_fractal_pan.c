#include "../src/fractal.h"
#include "../src/scroll.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static Parsed parsed;static Program pr;static EvalVars vars;
static uint16_t old[PLOT_W*PLOT_H],incremental[PLOT_W*PLOT_H],full[PLOT_W*PLOT_H];
typedef struct {uint16_t *pixels;int blocks;} Paint;
static void paint(int x,int y,int w,int h,int value,void *ctx)
{
    Paint *p=ctx;p->blocks++;
    assert(x>=PLOT_X0&&y>=PLOT_Y0&&x+w-1<=PLOT_X1&&y+h-1<=PLOT_Y1);
    for(int j=y;j<y+h;j++)for(int i=x;i<x+w;i++)p->pixels[(j-PLOT_Y0)*PLOT_W+i-PLOT_X0]=(uint16_t)value;
}
static int cancel(int done,int total,void *ctx){(void)total;int *n=ctx;(*n)++;return done<1;}
int main(void)
{
    assert(expr_parse(&parsed,"z^2+c",0)&&expr_compile(&parsed,&pr,NULL,0));
    int trials=0;
    for(int block=1;block<=6;block++)for(int axis=0;axis<4;axis++)for(int amount=15;amount<=45;amount+=30){
        /* Binary-exact scale/centers allow pixel-for-pixel comparison even
           around the sensitive Mandelbrot boundary. */
        FractalView a={.cx=-.5f,.cy=0.0f,.units_per_pixel=.00390625f,
            .escape_radius=4,.iterations=40,.block=block,.julia=0};
        Paint p={old,0};assert(fractal_generate(&pr,&vars,&a,paint,&p,NULL,NULL)==1);int initial=p.blocks;
        memcpy(incremental,old,sizeof old);
        int dx=axis==0?amount:axis==1?-amount:0,dy=axis==2?amount:axis==3?-amount:0;
        FractalView b=a;b.cx-=dx*a.units_per_pixel;b.cy+=dy*a.units_per_pixel;
        scroll_pixels(incremental,PLOT_W,PLOT_H,PLOT_W,dx,dy,0xffff);
        ScrollRect r[2];int n=scroll_exposed(PLOT_W,PLOT_H,dx,dy,r);p=(Paint){incremental,0};
        for(int i=0;i<n;i++)assert(fractal_generate_region(&pr,&vars,&b,
            PLOT_X0+r[i].x0,PLOT_Y0+r[i].y0,PLOT_X0+r[i].x1,PLOT_Y0+r[i].y1,
            dx,dy,paint,&p,NULL,NULL)==1);
        assert(p.blocks<initial);
        Paint q={full,0};assert(fractal_generate_region(&pr,&vars,&b,PLOT_X0,PLOT_Y0,PLOT_X1,PLOT_Y1,
                dx,dy,paint,&q,NULL,NULL)==1);
        assert(!memcmp(incremental,full,sizeof full));trials++;
        int calls=0;assert(fractal_generate_region(&pr,&vars,&b,PLOT_X0,PLOT_Y0,PLOT_X1,PLOT_Y1,
               dx,dy,paint,&q,cancel,&calls)==0&&calls>0);
    }
    /* Preserve lattice phase through a longer mixed sequence, including
       successive returns and partial blocks at multiple screen edges. */
    for(int block=1;block<=6;block++){
        FractalView view={.cx=-.5f,.cy=0,.units_per_pixel=.00390625f,
            .escape_radius=4,.iterations=40,.block=block,.julia=0};
        Paint p={incremental,0};assert(fractal_generate(&pr,&vars,&view,paint,&p,NULL,NULL)==1);
        int xs[]={15,0,-45,0,15,0,-15,45},ys[]={0,-45,0,15,0,45,0,-15};int phx=0,phy=0;
        for(int step=0;step<8;step++){
            int dx=xs[step],dy=ys[step];view.cx-=dx*view.units_per_pixel;view.cy+=dy*view.units_per_pixel;
            phx+=dx;phy+=dy;scroll_pixels(incremental,PLOT_W,PLOT_H,PLOT_W,dx,dy,0xffff);
            ScrollRect r[2];int n=scroll_exposed(PLOT_W,PLOT_H,dx,dy,r);p=(Paint){incremental,0};
            for(int i=0;i<n;i++)assert(fractal_generate_region(&pr,&vars,&view,
                PLOT_X0+r[i].x0,PLOT_Y0+r[i].y0,PLOT_X0+r[i].x1,PLOT_Y0+r[i].y1,
                phx,phy,paint,&p,NULL,NULL)==1);
            Paint q={full,0};assert(fractal_generate_region(&pr,&vars,&view,PLOT_X0,PLOT_Y0,PLOT_X1,PLOT_Y1,
                phx,phy,paint,&q,NULL,NULL)==1&&!memcmp(incremental,full,sizeof full));
        }
    }
    printf("Fractal translated-lattice strips near Mandelbrot boundary: %d single pans and 48 mixed sequence steps passed.\n",trials);
}
