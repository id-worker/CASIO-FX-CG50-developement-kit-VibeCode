/* The hardware-tested 1.63 engine, compiled alongside the new one. */
#define ww_init       ww163_init
#define ww_free       ww163_free
#define ww_clear      ww163_clear
#define ww_step       ww163_step
#define ww_load_text  ww163_load_text
#include "wireworld_163.c"
