/* GraphLab keyboard modifiers. Original project code: MIT; see LICENSE. */
#include "input.h"

char input_alpha_letter(int key) {
    switch(key) {
    case KEY_XOT:return 'a'; case KEY_LOG:return 'b'; case KEY_LN:return 'c';
    case KEY_SIN:return 'd'; case KEY_COS:return 'e'; case KEY_TAN:return 'f';
    case KEY_FRAC:return 'g'; case KEY_FD:return 'h'; case KEY_LEFTP:return 'i';
    case KEY_RIGHTP:return 'j'; case KEY_COMMA:return 'k'; case KEY_STORE:return 'l';
    case KEY_7:return 'm'; case KEY_8:return 'n'; case KEY_9:return 'o';
    case KEY_4:return 'p'; case KEY_5:return 'q'; case KEY_6:return 'r';
    case KEY_MUL:return 's'; case KEY_DIV:return 't'; case KEY_1:return 'u';
    case KEY_2:return 'v'; case KEY_3:return 'w'; case KEY_ADD:return 'x';
    case KEY_SUB:return 'y'; case KEY_0:return 'z'; default:return 0;
    }
}
void input_cycle_alpha(int *mode)
{
    if(*mode==INPUT_LOWER_LOCK)*mode=INPUT_UPPER_LOCK;
    else if(*mode==INPUT_UPPER_LOCK)*mode=INPUT_NUMBER;
    else *mode=INPUT_LOWER_LOCK;
}
/* gint forwards ALPHA itself (its ALPHA transform is disabled in input_key)
   but still applies delayed SHIFT. Check held SHIFT separately: gint's normal
   delayed modifier only annotates the first key in a held sequence. */
int input_apply(key_event_t *event,int *alpha_mode,int held_shift,int held_alpha) {
    if(event->type==KEYEV_OSMENU) return 0;
    if(held_shift) event->shift=1;
    if(event->key==KEY_ALPHA) {
        /* Keep GraphLab's previously documented SHIFT+ALPHA lock toggle. */
        if(event->shift) *alpha_mode=(*alpha_mode>=INPUT_LOWER_LOCK)?INPUT_NUMBER:INPUT_LOWER_LOCK;
        else if(*alpha_mode==INPUT_NUMBER) *alpha_mode=INPUT_ALPHA_ONCE;
        else if(*alpha_mode==INPUT_ALPHA_ONCE) *alpha_mode=INPUT_LOWER_LOCK;
        else *alpha_mode=INPUT_NUMBER;
        event->alpha=0;
        return 0;
    }
    int upper=(*alpha_mode==INPUT_UPPER_LOCK);
    if(held_alpha || (*alpha_mode && input_alpha_letter(event->key)))
        event->alpha=1;
    if(*alpha_mode==INPUT_ALPHA_ONCE) *alpha_mode=INPUT_NUMBER;
    return upper;
}
