/* Compiled against both 1.19 and 1.20 real GLAB8 storage implementations. */
#include "storage.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef GRAPHLAB_120
#define legacy_render_mode render3_mode
#define legacy_grid_div voxel_div
#endif
int main(int argc,char **argv)
{
    assert(argc==4);storage_set_test_dir(argv[1]);int oldmode=atoi(argv[3]);GraphState s;
    if(!strcmp(argv[2],"old-write")){
        state_defaults(&s);s.legacy_render_mode=(uint8_t)oldmode;s.legacy_grid_div=37;
        s.fast_ray_q=13;s.fast_ray_step=31;s.preview_ray_step=7;
        s.eye_x=11;s.eye_y=12;s.eye_z=13;s.map_extend_down=2;s.map_extend_up=5;
        strcpy(s.surface[0].formula,"z=x-y");assert(storage_save(&s)==0);
    }else if(!strcmp(argv[2],"new-read")){
        assert(storage_load(&s)==1);assert(s.legacy_render_mode==oldmode&&s.legacy_grid_div==37);
        assert(s.fast_ray_q==13&&s.fast_ray_step==31&&s.preview_ray_step==7);
        assert(s.eye_x==11&&s.eye_y==12&&s.eye_z==13&&s.map_extend_down==2&&s.map_extend_up==5);
        assert(!strcmp(s.surface[0].formula,"z=x-y"));
    }else if(!strcmp(argv[2],"new-update")){
        assert(storage_load(&s)==1&&s.legacy_render_mode==oldmode);
        s.fast_ray_q=27;s.fast_ray_step=19;s.preview_ray_step=4;s.eye_z=15;
        strcpy(s.surface[0].formula,"z=x+y");assert(storage_save(&s)==0);
    }else if(!strcmp(argv[2],"old-read")){
        assert(storage_load(&s)==1);assert(s.legacy_render_mode==oldmode&&s.legacy_grid_div==37);
        assert(s.fast_ray_q==27&&s.fast_ray_step==19&&s.preview_ray_step==4);
        assert(s.eye_x==11&&s.eye_y==12&&s.eye_z==15&&s.map_extend_down==2&&s.map_extend_up==5);
        assert(!strcmp(s.surface[0].formula,"z=x+y"));
    }else assert(0);
    return 0;
}
