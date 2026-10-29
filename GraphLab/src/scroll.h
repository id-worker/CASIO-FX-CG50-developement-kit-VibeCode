#ifndef GL_SCROLL_H
#define GL_SCROLL_H
#include <stdint.h>

typedef struct { int x0,y0,x1,y1; } ScrollRect;
/* Translate a rectangular image in-place. dx>0 moves the image right and
   dy>0 moves it down. The exposed area is filled, including for large moves.
   stride is in pixels and may exceed width. */
void scroll_pixels(uint16_t *pixels,int width,int height,int stride,
                   int dx,int dy,uint16_t clear);
/* Translate one tightly packed, row-major 1-bit image in place. */
void scroll_bits(unsigned char *bits,int width,int height,int dx,int dy);
/* Disjoint rectangles, in local coordinates, whose pixels have no source
   after the translation. Returns 0, 1 or 2. */
int scroll_exposed(int width,int height,int dx,int dy,ScrollRect out[2]);
#endif
