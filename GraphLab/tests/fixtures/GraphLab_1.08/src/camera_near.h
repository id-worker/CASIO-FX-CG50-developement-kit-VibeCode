#ifndef GL_CAMERA_NEAR_H
#define GL_CAMERA_NEAR_H
#include <math.h>
/* The domain cube and the camera's mathematical near plane are different:
   the former is fixed in world coordinates; the latter merely prevents
   division by zero/objects behind the eye. Keep it tiny in world units rather
   than letting it grow with the domain size. Radius is validated by callers. */
static inline float camera_near_normalized(float radius)
{
    return fminf(.04f,fmaxf(1e-9f,.00001f/radius));
}
#endif
