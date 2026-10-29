#ifndef GL_TRIGFAST_H
#define GL_TRIGFAST_H
#include "expr.h"
/* Optional acceleration for sums of independent sin/cos coordinates.
   Uses a bounded-error table only for values well away from zero; ambiguous
   samples and every unsupported expression fall back to the exact VM. */
typedef struct {
    float constant,sine[3],cosine[3],guard;
    int valid;
} TrigFast;
int trigfast_prepare(TrigFast *plan,Program const *program,EvalVars const *base,float bound);
int trigfast_value(TrigFast const *plan,float x,float y,float z,float target,float *value);
#endif
