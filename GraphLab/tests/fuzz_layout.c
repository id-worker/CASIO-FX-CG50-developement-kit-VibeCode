#include "../src/layout.h"
#include "host_gint.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint32_t seed=0x0daab453;
static uint32_t rand32(void){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
int main(void){
    static Parsed p;char s[EXPR_MAX];const char alphabet[]="xyzabctni0123456789.+-*/^=<>() ,sincosqr";
    unsigned layouts=0,positions=0;
    for(int rep=0;rep<100000;rep++){
        int len=(int)(rand32()%60);for(int i=0;i<len;i++)s[i]=alphabet[rand32()%(sizeof alphabet-1)];s[len]=0;
        if(!expr_parse(&p,s,1) || !layout_measure(&p))continue;
        layouts++;
        for(int k=0;k<=len;k++){
            FormulaCaret c=layout_draw(&p,k,0,0,C_BLACK,C_BLUE,1);
            if(!c.found){fprintf(stderr,"Missing cursor %d/%d in [%s]\n",k,len,s);return 1;}
            int node=-1,part=layout_sum_field(&p,k,&node);
            if(part < -1 || part > 3 || (part>=0 && (node<0 || node>=p.count)))abort();
            int next=layout_sum_step(&p,k,1),previous=layout_sum_step(&p,k,-1),del=layout_delete_start(&p,k);
            int comma=layout_sum_delimiter(&p,k,','),close=layout_sum_delimiter(&p,k,')');
            int group=layout_close_delimiter(&p,k),left=layout_cursor_step(&p,k,-1),right=layout_cursor_step(&p,k,1);
            if(next<0||next>len||previous<0||previous>len||del<0||del>k||comma < -1||comma>len||
               close < -1||close>len||group < -1||group>len||left<0||left>len||right<0||right>len)abort();
            positions++;
        }
    }
    char const *structured[]={"summ(0,inf,n,x^n/n!)","sqrt(summ(1,12,k,k!)/(x^2+1))",
      "summ(1,2,n,summ(3,5,k,n+k))","summ(0,inf,i,(sin(i)+cos(i))/3)",
      "0.5*summ(0,1,n,prod(1,5,k,k))","summ(,,,)","prod(,,,)",
      "((x+1)/(y-2))^(n+3)","y<=sqrt(x)","x^2+y^2>=1"};
    unsigned prefixes=0;
    for(unsigned j=0;j<sizeof structured/sizeof *structured;j++)for(int len=0;len<=(int)strlen(structured[j]);len++){
        memcpy(s,structured[j],(size_t)len);s[len]=0;
        if(!expr_parse(&p,s,1) || !layout_measure(&p))continue;
        prefixes++;for(int k=0;k<=len;k++){
            FormulaCaret c=layout_draw(&p,k,0,0,C_BLACK,C_BLUE,1);
            if(!c.found){fprintf(stderr,"Missing structured cursor %d/%d in [%s]\n",k,len,s);return 1;}
            int node=-1,part=layout_sum_field(&p,k,&node),next=layout_sum_step(&p,k,1),del=layout_delete_start(&p,k);
            if(part < -1 || part>3 || (part>=0 && (node<0||node>=p.count)) || next<0||next>len||del<0||del>k)abort();
        }
    }
    printf("Fuzzed layout: %u partial formulas, %u positions; %u structured prefixes.\n",layouts,positions,prefixes);
}
