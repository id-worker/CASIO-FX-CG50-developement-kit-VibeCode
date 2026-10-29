#include "../src/scroll.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

static unsigned random_state=984113u;
static unsigned rnd(void){random_state=random_state*1664525u+1013904223u;return random_state;}
static int getbit(unsigned char const *a,int i){return !!(a[i>>3]&(1u<<(i&7)));}
int main(void)
{
    int cases=0;
    for(int h=1;h<=19;h++)for(int w=1;w<=31;w++)for(int trial=0;trial<30;trial++){
        int stride=w+5,dx=(int)(rnd()%(2*w+5))-w-2,dy=(int)(rnd()%(2*h+5))-h-2;
        size_t np=(size_t)stride*h,nb=((size_t)w*h+7)/8;
        uint16_t *p=malloc((np+2)*sizeof *p),*old=malloc(np*sizeof *old);
        unsigned char *b=malloc(nb+2),*oldb=malloc(nb);
        assert(p&&old&&b&&oldb);p[0]=0x3579;p[np+1]=0x7531;b[0]=0x35;b[nb+1]=0x75;
        for(size_t i=0;i<np;i++)p[i+1]=(uint16_t)rnd();
        for(size_t i=0;i<nb;i++)b[i+1]=(unsigned char)rnd();
        memcpy(old,p+1,np*sizeof *old);memcpy(oldb,b+1,nb);
        scroll_pixels(p+1,w,h,stride,dx,dy,0xabcd);scroll_bits(b+1,w,h,dx,dy);
        ScrollRect rects[2];int count=scroll_exposed(w,h,dx,dy,rects);assert(count>=0&&count<=2);
        for(int y=0;y<h;y++)for(int x=0;x<w;x++){
            int sx=x-dx,sy=y-dy,inside=sx>=0&&sx<w&&sy>=0&&sy<h;
            assert(p[1+(size_t)y*stride+x]==(inside?old[(size_t)sy*stride+sx]:0xabcd));
            assert(getbit(b+1,y*w+x)==(inside?getbit(oldb,sy*w+sx):0));
            int cover=0;for(int k=0;k<count;k++)if(x>=rects[k].x0&&x<=rects[k].x1&&y>=rects[k].y0&&y<=rects[k].y1)cover++;
            assert(cover==!inside);
        }
        for(int y=0;y<h;y++)for(int x=w;x<stride;x++)assert(p[1+(size_t)y*stride+x]==old[(size_t)y*stride+x]);
        assert(p[0]==0x3579&&p[np+1]==0x7531&&b[0]==0x35&&b[nb+1]==0x75);
        free(p);free(old);free(b);free(oldb);cases++;
    }
    printf("In-place pixel/bit scrolling and exact exposed rectangles: %d cases passed.\n",cases);
}
