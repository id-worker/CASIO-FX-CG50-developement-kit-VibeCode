#define _POSIX_C_SOURCE 200809L
#include "../src/state.h"
#include "../src/storage.h"
#include "../src/layout.h"
#include "host_gint.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
int graphlab_test_editor(char *destination,int mode);
void graphlab_test_formula_area(char const *text,int cursor);
static int saw_limit;
void graphlab_editor_trace(char const *text,int cursor,char const *error)
{
    assert(strlen(text)<EXPR_MAX && cursor>=0 && cursor<=(int)strlen(text));
    if(strstr(error,"limit reached"))saw_limit=1;
}
static void balanced(char *s,int leaves)
{
    if(leaves==1){strcat(s,"1");return;}
    strcat(s,"(");balanced(s,leaves/2);strcat(s,"+");balanced(s,leaves-leaves/2);strcat(s,")");
}
static int visible_caret(void)
{
    for(int y=40;y<140;y++)for(int x=10;x<DWIDTH-10;x++)
        if(dgetpixel(x,y)==C_BLACK)return 1;
    return 0;
}
int main(void)
{
    assert(EXPR_MAX==768 && EXPR_NODES==256 && EXPR_CODE==256);
    static Parsed p;static Program program;char error[90];
    char formula[EXPR_MAX]="y=";balanced(formula,80);
    assert(strlen(formula)>300 && strlen(formula)<EXPR_MAX);
    assert(expr_parse(&p,formula,0) && p.count>128);
    assert(expr_compile(&p,&program,error,sizeof error) && program.size>128);
    EvalVars vars={0};vars.v[V_Y]=80;
    assert(expr_eval_real(&program,&vars)==0.0f);

    /* Script real keys beyond the former 159-byte limit. */
    enum {ZEROS=600};static key_event_t keys[ZEROS+3];
    keys[0]=(key_event_t){.key=KEY_0,.type=KEYEV_DOWN};
    keys[1]=(key_event_t){.key=KEY_DOT,.type=KEYEV_DOWN};
    for(int i=0;i<ZEROS;i++)keys[i+2]=(key_event_t){.key=KEY_0,.type=KEYEV_DOWN};
    keys[ZEROS+2]=(key_event_t){.key=KEY_EXE,.type=KEYEV_DOWN};
    char typed[EXPR_MAX]="";host_keys(keys,ZEROS+3);
    assert(graphlab_test_editor(typed,1) && strlen(typed)==ZEROS+2);
    assert(host_keys_used()==ZEROS+3 && host_vram_guards_ok());
    assert(expr_parse(&p,typed,0)&&expr_compile(&p,&program,error,sizeof error));

    /* Full capacity: reject one extra byte visibly; deleting then adding works. */
    char full[EXPR_MAX]="0.";memset(full+2,'0',EXPR_MAX-3);full[EXPR_MAX-1]=0;
    key_event_t at_limit[]={
        {.key=KEY_1,.type=KEYEV_DOWN},{.key=KEY_DEL,.type=KEYEV_DOWN},
        {.key=KEY_1,.type=KEYEV_DOWN},{.key=KEY_EXE,.type=KEYEV_DOWN}
    };
    saw_limit=0;host_keys(at_limit,sizeof at_limit/sizeof *at_limit);
    assert(graphlab_test_editor(full,1)&&strlen(full)==EXPR_MAX-1&&full[EXPR_MAX-2]=='1');
    assert(saw_limit && host_vram_guards_ok());

    /* Formatted long number: beginning, middle and end all expose a caret. */
    for(int i=0;i<3;i++){
        int pos=i==0?0:i==1?300:(int)strlen(typed);
        graphlab_test_formula_area(typed,pos);
        assert(visible_caret() && host_vram_guards_ok());
    }
    /* Fallback path for an unparseable formula: a caret must scroll with
       the byte position rather than remaining at the left edge. */
    char bad[EXPR_MAX];memset(bad,'0',700);bad[350]='@';bad[700]=0;
    int positions[]={0,1,159,350,699,700};
    for(size_t i=0;i<sizeof positions/sizeof *positions;i++){
        int pos=positions[i],x=14+pos*6;if(x>DWIDTH-22)x=DWIDTH-22;
        graphlab_test_formula_area(bad,pos);
        assert(dgetpixel(x,66)==C_BLACK && dgetpixel(x,75)==C_BLACK);
        assert(host_vram_guards_ok());
    }
    /* A cursor inside a function token is still anchored visibly. */
    graphlab_test_formula_area("sin(1)",2);
    assert(visible_caret());

    char dir[]="/tmp/gl-long-XXXXXX";assert(mkdtemp(dir));storage_set_test_dir(dir);
    GraphState state,out;state_defaults(&state);strcpy(state.curve[0].formula,formula);
    strcpy(state.recurrence[1],typed);assert(state_set_ray_quality(&state,17));
    assert(storage_save(&state)==0 && storage_load(&out)==1);
    assert(!strcmp(out.curve[0].formula,formula)&&!strcmp(out.recurrence[1],typed));
    assert(state_ray_quality(&out)==17);
    char file[256];snprintf(file,sizeof file,"%s/GLAB4_A.DAT",dir);unlink(file);assert(rmdir(dir)==0);
    puts("Long editor: >600 keys, 767-byte boundary, parsed/raw caret following, >128 VM instructions and v5 persistence passed.");
}
