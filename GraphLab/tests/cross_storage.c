#include "storage.h"
#ifdef GL_CURRENT_120
#define voxel_div legacy_grid_div
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(int argc,char **argv){
    if(argc!=3)return 2;
    storage_set_test_dir(argv[1]);GraphState s;
    if(!strcmp(argv[2],"write-old")){
        state_defaults(&s);strcpy(s.curve[0].formula,"y=sin(x)+4");
        strcpy(s.surface[0].formula,"x^2+y^2+z^2=9");
        s.volume_radius=8;s.eye_x=4.25f;s.voxel_div=32;s.accuracy2=3;s.follow_eye=1;
        assert(storage_save(&s)==0);
    }else if(!strcmp(argv[2],"read-old")){
        assert(storage_load(&s)==1);assert(!strcmp(s.curve[0].formula,"y=sin(x)+4"));
        assert(!strcmp(s.surface[0].formula,"x^2+y^2+z^2=9"));
        assert(s.volume_radius==8 && s.eye_x==4.25f && s.voxel_div==32 && s.accuracy2==3 && s.follow_eye==1);
    }else if(!strcmp(argv[2],"write-new")){
        assert(storage_load(&s)==1);strcpy(s.recurrence[2],"z^4+c");
        s.volume_radius=12;s.detail3=3;s.voxel_div=64;assert(storage_save(&s)==0);
    }else if(!strcmp(argv[2],"read-new")){
        assert(storage_load(&s)==1);
        assert(!strcmp(s.recurrence[2],"z^4+c") && s.volume_radius==12 && s.detail3==3 && s.voxel_div==64);
    }else return 3;
    printf("%s: verified\n",argv[2]);return 0;
}
