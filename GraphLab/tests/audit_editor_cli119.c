/* Scriptable actual editor for baseline traces and synthetic VRAM shots.
   Build with GL_EDITOR_TEST and GL_EDITOR_TRACE. See run_baseline_editor119.sh. */
#include "host_gint.h"
#include "state.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int graphlab_test_editor(char *destination,int mode);
void graphlab_editor_trace(char const *s,int c,char const *err){
    printf("%3d  %.*s|%s",c,c,s,s+c);if(*err)printf(" [ %s ]",err);puts("");
}
static int key(char const *s){
#define MAP(x) if(!strcmp(s,#x))return KEY_##x
    MAP(DEL);MAP(RIGHT);MAP(LEFT);MAP(UP);MAP(DOWN);MAP(LEFTP);MAP(RIGHTP);MAP(SIN);MAP(COS);MAP(TAN);MAP(LOG);MAP(LN);MAP(EXE);MAP(EXIT);MAP(FRAC);MAP(DIV);MAP(ADD);MAP(SUB);MAP(MUL);MAP(POWER);MAP(SQUARE);MAP(XOT);MAP(COMMA);MAP(F1);MAP(F2);MAP(F3);MAP(F4);MAP(F5);MAP(F6);MAP(ALPHA);MAP(OPTN);MAP(0);MAP(1);MAP(2);MAP(3);MAP(4);MAP(5);MAP(6);MAP(7);MAP(8);MAP(9);MAP(DOT);
    fprintf(stderr,"Unknown key %s\n",s);exit(1);
}
int main(int argc,char **argv){if(argc<2)return 1;int mode=getenv("MODE")?atoi(getenv("MODE")):1;
 char dest[EXPR_MAX];snprintf(dest,sizeof dest,"%s",argv[1]);
 key_event_t keys[600];size_t n=0;
 for(int i=2;i<argc;i++){char const *s=argv[i];int shift=0,alpha=0;if(!strncmp(s,"S-",2)){shift=1;s+=2;}if(!strncmp(s,"A-",2)){alpha=1;s+=2;}keys[n++]=(key_event_t){.key=key(s),.type=KEYEV_DOWN,.mod=shift||alpha,.shift=shift,.alpha=alpha};}
 keys[n++]=(key_event_t){.key=KEY_EXIT,.type=KEYEV_DOWN};host_keys(keys,n);
 if(getenv("SNAP")&&getenv("SNAP_KEY"))host_snapshot_on_getkey((size_t)atoi(getenv("SNAP_KEY")),getenv("SNAP"));
 int ok=graphlab_test_editor(dest,mode);printf("accepted=%d stored=%s\n",ok,dest);return 0;}
