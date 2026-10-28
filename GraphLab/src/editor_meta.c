#include "editor_meta.h"
#include <string.h>

static int stack_before(const char *text,int at,int16_t *stack)
{
    int depth=0;
    for(int i=0;i<at && text[i];i++){
        if(text[i]=='('){if(depth<EXPR_MAX)stack[depth++]=(int16_t)i;}
        else if(text[i]==')' && depth>0)depth--;
    }
    return depth;
}
int editor_meta_open_before(const char *text,int cursor)
{
    int16_t stack[EXPR_MAX];int n=stack_before(text,cursor,stack);
    return n?stack[n-1]:-1;
}
static void pair(EditorMeta *m,int open,int close)
{
    if(open<0 || open>=EXPR_MAX || close<0 || close>=EXPR_MAX)return;
    int old=m->mate[open];
    if(old>=0 && old<EXPR_MAX)m->mate[old]=-1;
    old=m->mate[close];
    if(old>=0 && old<EXPR_MAX)m->mate[old]=-1;
    m->mate[open]=(int16_t)close;m->mate[close]=(int16_t)open;
}
void editor_meta_init(EditorMeta *m,const char *text)
{
    for(int i=0;i<EXPR_MAX;i++)m->mate[i]=m->comma_owner[i]=-1;
    int16_t stack[EXPR_MAX];int depth=0;
    for(int i=0;text[i] && i<EXPR_MAX-1;i++){
        if(text[i]=='(')stack[depth++]=(int16_t)i;
        else if(text[i]==',' && depth>0)m->comma_owner[i]=stack[depth-1];
        else if(text[i]==')' && depth>0)pair(m,stack[--depth],i);
    }
}
static int16_t moved(int16_t pos,int at,int n)
{
    return pos<0?pos:(int16_t)(pos>=at?pos+n:pos);
}
void editor_meta_insert(EditorMeta *m,const char *text,int at,const char *ins)
{
    int len=(int)strlen(text),n=(int)strlen(ins);
    if(at<0 || at>len || n<=0 || len+n>=EXPR_MAX)return;
    int16_t stack[EXPR_MAX];int depth=stack_before(text,at,stack);
    for(int i=len-1;i>=at;i--){
        m->mate[i+n]=moved(m->mate[i],at,n);
        m->comma_owner[i+n]=moved(m->comma_owner[i],at,n);
    }
    for(int i=0;i<at;i++){
        m->mate[i]=moved(m->mate[i],at,n);
        m->comma_owner[i]=moved(m->comma_owner[i],at,n);
    }
    for(int i=0;i<n;i++)m->mate[at+i]=m->comma_owner[at+i]=-1;
    for(int i=0;i<n;i++){
        int pos=at+i;
        if(ins[i]=='(')stack[depth++]=(int16_t)pos;
        else if(ins[i]==',' && depth>0)m->comma_owner[pos]=stack[depth-1];
        else if(ins[i]==')' && depth>0)pair(m,stack[--depth],pos);
    }
}
static int16_t removed(int16_t pos,int start,int end)
{
    if(pos<0)return pos;
    if(pos>=start && pos<end)return -1;
    return (int16_t)(pos>=end?pos-(end-start):pos);
}
void editor_meta_delete(EditorMeta *m,int len,int start,int end)
{
    if(start<0 || end<start || end>len || len>=EXPR_MAX)return;
    int n=end-start;if(!n)return;
    for(int i=0;i<len;i++){
        if(i>=start && i<end)continue;
        int at=i>=end?i-n:i;
        m->mate[at]=removed(m->mate[i],start,end);
        m->comma_owner[at]=removed(m->comma_owner[i],start,end);
    }
    for(int i=len-n;i<len;i++)m->mate[i]=m->comma_owner[i]=-1;
}
int editor_meta_skip(const EditorMeta *m,const char *text,int cursor,char delimiter)
{
    int len=(int)strlen(text);
    if(cursor<0 || cursor>=len || text[cursor]!=delimiter)return 0;
    int open=editor_meta_open_before(text,cursor);
    if(open<0)return 0;
    if(delimiter==')')return m->mate[cursor]==open;
    if(delimiter==',')return m->comma_owner[cursor]==open;
    return 0;
}
int editor_meta_can_cross(const EditorMeta *m,const char *text,int from,int to)
{
    int len=(int)strlen(text);
    if(from<0 || to<from || to>len)return 0;
    for(int i=from;i<to;i++)if(text[i]==')' && m->mate[i]>=0){
        int open=editor_meta_open_before(text,i);
        if(open>=0 && m->mate[i]!=open)return 0;
    }
    return 1;
}
