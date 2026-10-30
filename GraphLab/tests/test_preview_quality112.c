#include "../src/preview3d.h"
#include "host_gint.h"
#include <assert.h>
#include <stdio.h>
static int capture(int step)
{
    Preview3D p={0};Volume v={.radius=3,.div=20};Camera c={.zoom=1,.radius=3,.eye_y=6,.perspective=1};
    preview3d_set_step(&p,step);assert(preview3d_begin(&p,&v,&c)&&p.step==step);
    for(int y=PLOT_Y0;y<=PLOT_Y1;y++)for(int x=PLOT_X0;x<=PLOT_X1;x++)
        preview3d_add(&p,x,y,0,0,0,C_BLUE);
    assert(p.count==p.capacity);int count=p.count;
    dclear(C_WHITE);assert(preview3d_draw(&p,&c)==1&&host_vram_guards_ok());
    preview3d_free(&p);return count;
}
int main(void)
{
    host_keys(NULL,0);int fine=capture(2),normal=capture(4),fast=capture(8);
    assert(fine>normal*3&&normal>fast*3);
    /* Changing a live cache cannot reuse an allocation sized for another step. */
    Preview3D p={0};Volume v={.radius=3,.div=20};Camera c={.zoom=1,.radius=3,.eye_y=6,.perspective=1};
    preview3d_set_step(&p,4);assert(preview3d_begin(&p,&v,&c));preview3d_set_step(&p,2);
    assert(!p.point&&p.step==2&&preview3d_recapture(&p,&v,&c,1)&&p.step==2);
    preview3d_free(&p);
    printf("Point-preview sampling: 2 px %d, original 4 px %d, 8 px %d; depth/VRAM guards passed.\n",fine,normal,fast);
}
