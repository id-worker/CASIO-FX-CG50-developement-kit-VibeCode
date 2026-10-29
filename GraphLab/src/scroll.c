/* In-place image and bit-mask translation; original GraphLab code, MIT. */
#include "scroll.h"
#include <stddef.h>
#include <string.h>

void scroll_pixels(uint16_t *p,int w,int h,int stride,int dx,int dy,uint16_t clear)
{
    if(!p||w<=0||h<=0||stride<w)return;
    if(dx<=-w||dx>=w||dy<=-h||dy>=h){
        for(int y=0;y<h;y++)for(int x=0;x<w;x++)p[(size_t)y*stride+x]=clear;
        return;
    }
    int y=dy>0?h-1:0,stop=dy>0?-1:h,step=dy>0?-1:1;
    for(;y!=stop;y+=step){
        uint16_t *dst=p+(size_t)y*stride;int sy=y-dy;
        if(sy<0||sy>=h){for(int x=0;x<w;x++)dst[x]=clear;continue;}
        uint16_t const *src=p+(size_t)sy*stride;
        int left=dx>0?dx:0,source=dx<0?-dx:0,count=w-(dx<0?-dx:dx);
        memmove(dst+left,src+source,(size_t)count*sizeof *dst);
        for(int x=0;x<left;x++)dst[x]=clear;
        for(int x=left+count;x<w;x++)dst[x]=clear;
    }
}
static int getbit(unsigned char const *b,size_t i){return !!(b[i>>3]&(1u<<(i&7)));}
static void putbit(unsigned char *b,size_t i,int value)
{
    unsigned char mask=(unsigned char)(1u<<(i&7));
    if(value)b[i>>3]|=mask;else b[i>>3]&=(unsigned char)~mask;
}
void scroll_bits(unsigned char *b,int w,int h,int dx,int dy)
{
    if(!b||w<=0||h<=0)return;
    if(dx<=-w||dx>=w||dy<=-h||dy>=h){memset(b,0,((size_t)w*h+7)/8);return;}
    int y=dy>0?h-1:0,ystop=dy>0?-1:h,ys=dy>0?-1:1;
    for(;y!=ystop;y+=ys){
        int x=dx>0?w-1:0,xstop=dx>0?-1:w,xs=dx>0?-1:1;
        for(;x!=xstop;x+=xs){
            int sx=x-dx,sy=y-dy;
            int value=(sx>=0&&sx<w&&sy>=0&&sy<h)?getbit(b,(size_t)sy*w+sx):0;
            putbit(b,(size_t)y*w+x,value);
        }
    }
}
int scroll_exposed(int w,int h,int dx,int dy,ScrollRect out[2])
{
    if(!out||w<=0||h<=0)return 0;
    if(dx<=-w||dx>=w||dy<=-h||dy>=h){out[0]=(ScrollRect){0,0,w-1,h-1};return 1;}
    int n=0,top=0,bottom=h-1;
    if(dy>0){out[n++]=(ScrollRect){0,0,w-1,dy-1};top=dy;}
    else if(dy<0){out[n++]=(ScrollRect){0,h+dy,w-1,h-1};bottom=h+dy-1;}
    if(dx>0)out[n++]=(ScrollRect){0,top,dx-1,bottom};
    else if(dx<0)out[n++]=(ScrollRect){w+dx,top,w-1,bottom};
    return n;
}
