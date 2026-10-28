#ifndef GL_PLATFORM_H
#define GL_PLATFORM_H
#ifdef GL_HOST
#include "../tests/host_gint.h"
#else
#include <gint/display.h>
#include <gint/keyboard.h>
#include <gint/gint.h>
#endif
#endif
