#ifndef GL_STORAGE_H
#define GL_STORAGE_H
#include "state.h"
/* Each update writes a second journal slot before replacing an older one.
 * Files not recognized as ours are never overwritten. */
int storage_load(GraphState *state);
int storage_save(GraphState const *state);
#ifdef GL_HOST
/* Tests use a private directory rather than the calculator's storage root. */
void storage_set_test_dir(char const *path);
#endif
#endif
