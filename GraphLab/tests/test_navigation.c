#include "../src/navigation.h"
#include "../src/navigation_input.h"
#include "../src/storage.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static int near(float x,float y){return fabsf(x-y)<1e-4f;}
static float distance(GraphState const *a,GraphState const *b){
    return sqrtf((a->eye_x-b->eye_x)*(a->eye_x-b->eye_x)+
                 (a->eye_y-b->eye_y)*(a->eye_y-b->eye_y)+
                 (a->eye_z-b->eye_z)*(a->eye_z-b->eye_z));
}
int main(void){
    GraphState orig,normal,slow,fast;state_defaults(&orig);
    normal=slow=fast=orig;
    assert(navigation_step(&normal,NAV_FORWARD|NAV_RIGHT,0,0));
    assert(navigation_step(&slow,NAV_FORWARD|NAV_RIGHT,1,0));
    assert(navigation_step(&fast,NAV_FORWARD|NAV_RIGHT,0,1));
    assert(near(normal.camera_yaw,orig.camera_yaw-.065f));
    assert(near(slow.camera_yaw,normal.camera_yaw)&&near(fast.camera_yaw,normal.camera_yaw));
    assert(near(distance(&slow,&orig),.2f*distance(&normal,&orig)));
    assert(near(distance(&fast,&orig),3.0f*distance(&normal,&orig)));
    assert(normal.eye_x<orig.eye_x && normal.eye_y<orig.eye_y && normal.eye_z<orig.eye_z);
    normal=orig;assert(navigation_step(&normal,NAV_RISE,0,0));
    assert(near(normal.eye_x,orig.eye_x)&&near(normal.eye_y,orig.eye_y));
    assert(near(normal.eye_z,orig.eye_z+.065f*orig.volume_radius));
    assert(navigation_step(&normal,NAV_FALL,0,0));assert(fabsf(normal.eye_z-orig.eye_z)<.01f);
    normal=orig;assert(!navigation_step(&normal,NAV_LEFT|NAV_RIGHT|NAV_FORWARD|NAV_BACK,0,0));
    assert(!memcmp(&normal,&orig,sizeof orig));
    normal=orig;assert(navigation_step(&normal,NAV_SIDE_RIGHT|NAV_FORWARD|NAV_RISE,0,0));
    assert(normal.eye_z>orig.eye_z-.065f*orig.volume_radius);
    normal=orig;normal.follow_eye=1;normal.volume_x=2;normal.volume_y=-1;normal.volume_z=3;
    assert(navigation_step(&normal,NAV_FORWARD|NAV_SIDE_LEFT|NAV_RISE,0,0));
    assert(normal.volume_x==2 && normal.volume_y==-1 && normal.volume_z==3);
    assert(navigation_key(KEY_ADD)==0 && navigation_key(KEY_SUB)==0);
    /* Near the origin, even a huge cube must not force enormous steps. */
    normal=orig;normal.volume_radius=10000;normal.eye_x=normal.eye_z=0;normal.eye_y=.1f;
    assert(navigation_step(&normal,NAV_RISE,0,0));
    assert(normal.eye_z>0 && normal.eye_z<.01f);
    normal=orig;normal.volume_radius=1000;state_default_eye(&normal);
    assert(near(sqrtf(normal.eye_x*normal.eye_x+normal.eye_y*normal.eye_y+normal.eye_z*normal.eye_z),8.4f));
    normal.volume_radius=.1f;state_default_eye(&normal);
    assert(near(sqrtf(normal.eye_x*normal.eye_x+normal.eye_y*normal.eye_y+normal.eye_z*normal.eye_z),.28f));
    normal=orig;normal.camera_pitch=1.5f;assert(!navigation_step(&normal,NAV_DOWN,1,0));
    normal=orig;normal.camera_yaw=6.28f;assert(navigation_step(&normal,NAV_LEFT,0,0));
    assert(normal.camera_yaw<.1f && state_validate(&normal));
    puts("3D navigation: independent axes, adaptive movement, fixed domain and no 3D +/- passed.");
    return 0;
}
