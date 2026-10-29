/* Inject failures into the temporary editor sessions, including nesting. */
#include "../src/expr.h"
#include "../src/editor_meta.h"
#include "host_gint.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int graphlab_test_editor(char *destination,int mode);
char const *graphlab_test_notice(void);
void *__real_malloc(size_t);void __real_free(void *);
typedef struct {char text[EXPR_MAX],error[90];EditorMeta meta;} SessionSize;
static int attempt,fail_attempt,live,created,freed;
static void *sessions[4];
void *__wrap_malloc(size_t n)
{
    if(n==sizeof(SessionSize)){
        attempt++;if(attempt==fail_attempt)return NULL;
        void *p=__real_malloc(n);assert(p && live<4);sessions[live++]=p;created++;return p;
    }
    return __real_malloc(n);
}
void __wrap_free(void *p)
{
    for(int i=0;i<live;i++)if(sessions[i]==p){sessions[i]=sessions[--live];freed++;break;}
    __real_free(p);
}
static void reset(int fail){assert(live==0);attempt=created=freed=0;fail_attempt=fail;}
#define K(k) {.key=KEY_##k,.type=KEYEV_DOWN}
#define N(a) (sizeof(a)/sizeof(*(a)))
int main(void)
{
    char text[EXPR_MAX]="y=sin(x)", original[EXPR_MAX];strcpy(original,text);
    reset(1);host_keys(NULL,0);assert(!graphlab_test_editor(text,1));
    assert(!strcmp(text,original) && strstr(graphlab_test_notice(),"not enough memory"));
    assert(attempt==1 && live==0 && created==0);
    reset(0);key_event_t cancel[]={K(EXIT)};host_keys(cancel,N(cancel));
    assert(!graphlab_test_editor(text,1) && !strcmp(text,original));
    assert(created==1 && freed==1 && live==0);
    /* Radius editing attempts an inner editor while the original remains open. */
    reset(2);key_event_t fail_inner[]={K(OPTN),K(DOWN),K(EXE),K(EXIT),K(EXE)};
    host_keys(fail_inner,N(fail_inner));assert(graphlab_test_editor(text,1));
    assert(!strcmp(text,original) && attempt==2 && created==1 && freed==1 && live==0);
    assert(host_keys_used()==N(fail_inner));
    reset(0);key_event_t nested[]={K(OPTN),K(DOWN),K(EXE),K(EXE),K(EXIT),K(EXE)};
    host_keys(nested,N(nested));assert(graphlab_test_editor(text,1));
    assert(!strcmp(text,original) && attempt==2 && created==2 && freed==2 && live==0);
    assert(host_keys_used()==N(nested) && host_vram_guards_ok());
    puts("Editor low memory: outer/inner allocation failure, two nested sessions and cleanup passed.");
}
