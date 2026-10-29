#include "storage.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(int argc,char **argv)
{
    assert(argc==3);storage_set_test_dir(argv[1]);GraphState s;
#ifndef GL_109
    if(!strcmp(argv[2],"old-write")){
        state_defaults(&s);assert(state_set_ray_quality(&s,17));
        strcpy(s.curve[0].formula,"y=x+6");assert(storage_save(&s)==0);
    }else if(!strcmp(argv[2],"old-read-own")){
        assert(storage_load(&s)==1&&state_ray_quality(&s)==17);
        assert(!strcmp(s.curve[0].formula,"y=x+6"));
        strcpy(s.curve[0].formula,"y=x+9");assert(state_set_ray_quality(&s,18));assert(storage_save(&s)==0);
    }
#else
    if(!strcmp(argv[2],"new-import")){
        assert(storage_load(&s)==1&&state_ray_quality(&s)==17);
        assert(!strcmp(s.curve[0].formula,"y=x+6"));
        assert(state_set_ray_quality(&s,35));strcpy(s.curve[0].formula,"y=x+8");
        assert(storage_save(&s)==0);
    }else if(!strcmp(argv[2],"new-untrusted")){
        assert(storage_load(&s)==1&&state_ray_quality(&s)==20);
        assert(!strcmp(s.curve[0].formula,"y=x+6"));
    }else if(!strcmp(argv[2],"new-still-own")){
        assert(storage_load(&s)==1&&state_ray_quality(&s)==35);
        assert(!strcmp(s.curve[0].formula,"y=x+8"));
    }
#endif
    else return 3;
    printf("%s verified\n",argv[2]);return 0;
}
