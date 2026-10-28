#include "../src/editor_meta.h"
#include "../src/layout.h"
#include "host_gint.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void insert(char *s,EditorMeta *m,int at,const char *ins)
{
    int len=(int)strlen(s),n=(int)strlen(ins);assert(at>=0&&at<=len&&len+n<EXPR_MAX);
    editor_meta_insert(m,s,at,ins);
    memmove(s+at+n,s+at,(size_t)(len-at+1));memcpy(s+at,ins,(size_t)n);
}
static void erase(char *s,EditorMeta *m,int a,int b)
{
    int len=(int)strlen(s);assert(a>=0&&a<=b&&b<=len);
    editor_meta_delete(m,len,a,b);memmove(s+a,s+b,(size_t)(len-b+1));
}
static void invariant(const char *s,const EditorMeta *m)
{
    int len=(int)strlen(s);
    for(int i=0;i<len;i++){
        int j=m->mate[i];
        if(j>=0){assert(j<len && m->mate[j]==i);assert((s[i]=='(' && s[j]==')')||(s[i]==')' && s[j]=='('));}
        j=m->comma_owner[i];
        if(j>=0){assert(j<len && s[i]==',' && s[j]=='(' && j<i);}
    }
}
static Parsed p;
static unsigned rnd=0x518af3b1u;
static unsigned next(void){rnd^=rnd<<13;rnd^=rnd>>17;rnd^=rnd<<5;return rnd;}
int main(void)
{
    char s[EXPR_MAX]="summ(0,inf,n,)";EditorMeta m;editor_meta_init(&m,s);
    int body=(int)strlen(s)-1, outer_close=body,outer_open=4;
    insert(s,&m,body,"(");body++;
    assert(m.mate[outer_close+1]==outer_open); /* opening is not auto-paired */
    assert(!editor_meta_skip(&m,s,body,')'));
    assert(expr_parse_editor(&p,s,m.mate,m.comma_owner) && layout_measure(&p));
    ExprNode const *sum=&p.nodes[p.root],*group=&p.nodes[sum->extra];
    assert(sum->kind==N_SUM && sum->op_pos>=0 && group->kind==N_GROUP && group->right<0);
    insert(s,&m,body,"x");body++;
    assert(!editor_meta_skip(&m,s,body,')')); /* that closer still belongs to SUM */
    insert(s,&m,body,")");body++;
    assert(editor_meta_skip(&m,s,body,')'));
    assert(!strcmp(s,"summ(0,inf,n,(x))"));assert(expr_parse(&p,s,0));invariant(s,&m);
    /* A function's comma must not steal the pre-existing outer SUM comma. */
    strcpy(s,"summ(0,,n,x)");editor_meta_init(&m,s);
    int at=7;insert(s,&m,at,"min(");at+=4;insert(s,&m,at,"1");at++;
    assert(s[at]==',' && m.comma_owner[at]==4 && !editor_meta_skip(&m,s,at,','));
    assert(expr_parse_editor(&p,s,m.mate,m.comma_owner) && layout_measure(&p));
    assert(p.nodes[p.root].kind==N_SUM && p.nodes[p.nodes[p.root].right].kind==N_FUNC);
    insert(s,&m,at,",");at++;insert(s,&m,at,"2");at++;insert(s,&m,at,")");at++;
    assert(editor_meta_skip(&m,s,at,','));
    assert(!strcmp(s,"summ(0,min(1,2),n,x)"));assert(expr_parse(&p,s,0));invariant(s,&m);
    /* Random insertions/deletions preserve anchor integrity and safe parses. */
    static const char *const pieces[]={"(",")",",","x","1","+","/()","sin()","summ(,,,)","min(,)","^()"};
    s[0]=0;editor_meta_init(&m,s);
    for(int i=0;i<120000;i++){
        int len=(int)strlen(s);unsigned r=next();
        if((r&3u) && len<125){
            const char *v=pieces[next()%(sizeof pieces/sizeof *pieces)];
            if(len+(int)strlen(v)<EXPR_MAX)insert(s,&m,(int)(next()%(unsigned)(len+1)),v);
        }else if(len){int a=(int)(next()%(unsigned)len);int b=a+1+(int)(next()%(unsigned)(len-a));erase(s,&m,a,b);}
        invariant(s,&m);
        if(expr_parse_editor(&p,s,m.mate,m.comma_owner)){
            assert(layout_measure(&p));
            int at=(int)(next()%(strlen(s)+1));
            (void)layout_fraction_step(&p,at,1);(void)layout_fraction_step(&p,at,-1);
            (void)layout_draw(&p,at,0,0,C_BLACK,C_BLUE,1);
        }
    }
    puts("Editor delimiter ownership, unclosed manual groups and 120000 metadata mutations passed.");
}
