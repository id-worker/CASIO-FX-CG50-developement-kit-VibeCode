#ifndef GL_LAYOUT_H
#define GL_LAYOUT_H
#include "expr.h"
typedef struct { int x, y, h, found; } FormulaCaret;
/* Computes dimensions stored in nodes; returns 0 for invalid tree. */
int layout_measure(Parsed *p);
/* Draw or measure caret for the formula, relative to x,y. */
FormulaCaret layout_draw(Parsed const *p, int cursor, int x, int y, int ink, int cursor_color, int dry_run);
/* Step to the next visible caret location, skipping hidden token letters. */
int layout_cursor_step(Parsed const *p,int cursor,int direction);
/* Innermost fraction: 0 numerator, 1 denominator, -1 outside. */
int layout_fraction_field(Parsed const *p,int cursor,int *fraction_node);
int layout_fraction_step(Parsed const *p,int cursor,int direction);
/* SUM fields: 0 lower, 1 upper, 2 index, 3 expression; -1 outside. */
int layout_sum_field(Parsed const *p,int cursor,int *sum_node);
int layout_sum_step(Parsed const *p,int cursor,int direction);
/* Reuse an existing SUM/PROD comma or closing parenthesis; -1 means insert. */
int layout_sum_delimiter(Parsed const *p,int cursor,char delimiter);
/* Reuse an existing closing ')' for a visible group/function, or -1. */
int layout_close_delimiter(Parsed const *p,int cursor);
int layout_delete_start(Parsed const *p,int cursor);
#endif
