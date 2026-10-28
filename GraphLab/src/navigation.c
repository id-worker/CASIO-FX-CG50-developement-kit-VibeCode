/* 3D camera movement adapted to GraphLab's Z-up coordinates.
   Original GraphLab code, MIT; see LICENSE. No triangle/polygon renderer. */
#include "navigation.h"
#include <math.h>
static float clamp(float v,float lo,float hi){return fmaxf(lo,fminf(hi,v));}
int navigation_step(GraphState *s,uint32_t held,int slow,int fast)
{
    if(!s)return 0;
    float oyaw=s->camera_yaw,opitch=s->camera_pitch;
    float ex=s->eye_x,ey=s->eye_y,ez=s->eye_z;
    /* GraphLab's angle points from the target toward the camera, opposite to
       the view direction. These signs match physical LEFT/RIGHT/UP/DOWN. */
    s->camera_yaw+=.065f*((!!(held&NAV_LEFT))-(!!(held&NAV_RIGHT)));
    s->camera_pitch=clamp(s->camera_pitch+.065f*((!!(held&NAV_DOWN))-(!!(held&NAV_UP))),-1.5f,1.5f);
    if(s->camera_yaw>6.283185307f)s->camera_yaw-=6.283185307f;
    if(s->camera_yaw< -6.283185307f)s->camera_yaw+=6.283185307f;
    float cy=cosf(s->camera_yaw),sy=sinf(s->camera_yaw);
    float cp=cosf(s->camera_pitch),sp=sinf(s->camera_pitch);
    float distance=sqrtf(ex*ex+ey*ey+ez*ez);
    float basis=fminf(s->volume_radius,fmaxf(distance/2.8f,.001f));
    float move=basis*.065f*(fast?3.0f:slow?.20f:1.0f);
    int forward=(!!(held&NAV_FORWARD))-(!!(held&NAV_BACK));
    int side=(!!(held&NAV_SIDE_RIGHT))-(!!(held&NAV_SIDE_LEFT));
    int height=(!!(held&NAV_RISE))-(!!(held&NAV_FALL));
    s->eye_x=clamp(ex+move*(-forward*cp*sy+side*cy),-1e6f,1e6f);
    s->eye_y=clamp(ey+move*(-forward*cp*cy-side*sy),-1e6f,1e6f);
    s->eye_z=clamp(ez+move*(-forward*sp+height),-1e6f,1e6f);
    return s->camera_yaw!=oyaw || s->camera_pitch!=opitch ||
           s->eye_x!=ex || s->eye_y!=ey || s->eye_z!=ez;
}
