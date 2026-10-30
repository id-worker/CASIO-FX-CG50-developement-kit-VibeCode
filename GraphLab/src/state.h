#ifndef GL_STATE_H
#define GL_STATE_H
#include "expr.h"
#include "fractal.h"
#define CURVE_COUNT 3
#define SURFACE_COUNT 2
#define FRACTAL_COUNT 3
typedef struct { char formula[EXPR_MAX]; uint8_t active; } Equation;
typedef struct {
    Equation curve[CURVE_COUNT];
    Equation surface[SURFACE_COUNT];
    char recurrence[FRACTAL_COUNT][EXPR_MAX];
    float param_a,param_b,param_c,param_t;
    float plane_x,plane_y,plane_scale;
    float volume_x,volume_y,volume_z,volume_radius;
    float camera_yaw,camera_pitch,camera_zoom;
    FractalView fractal;
    uint8_t grid,detail2,detail3,wire,axes,fractal_slot;
    /* Version 2 fields are appended so version 1 saves can be migrated. */
    float eye_x,eye_y,eye_z,period_x;
    uint8_t axes_front;
    /* Explicit former v2 tail padding; v3 fields start after the old state. */
    uint8_t reserved_v2[3];
    uint8_t curve_width; /* 1..5 pixels */
    uint8_t reserved_v3[3]; /* transient checked Q; authenticated by 1.08 sidecar */
    /* Former mode/grid bytes retained only for GLAB8 and older journal ABI.
       They do not select an engine in 1.20. Old add-ins can still read them. */
    uint8_t legacy_render_mode;
    uint8_t legacy_grid_div;
    uint8_t accuracy2; /* 0,1,2,3 => 0.25,0.5,1,2 screen pixels */
    uint8_t follow_eye; /* optionally move the search cube with translation */
    /* v6: settings for the independent intersection and point-preview modes. */
    float map_zmin,map_zmax;
    uint8_t map_xy_quality,map_z_quality,cursor_quality,preview_quality;
    /* v7 journal: independent starting step and Q for the Ray renderer. */
    uint8_t fast_ray_step,fast_ray_q;
    float map_extend_down,map_extend_up; /* optional world units beyond +/- clip */
    /* v8 journal: direct 3D preview screen-pixel spacing, independent of the starting Ray step. */
    uint8_t preview_ray_step; /* 1..32 */
} GraphState;
void state_defaults(GraphState *s);
void state_default_eye(GraphState *s);
/* Q is carried in previously reserved v3 bytes while detail3 remains a
   valid old-version preset. A matching 1.08 sidecar authenticates it on load;
   arbitrary old padding or a save changed by an old version falls back. */
int state_ray_quality(GraphState const *s);
int state_set_ray_quality(GraphState *s,int q);
int state_validate(GraphState const *s);
#endif
