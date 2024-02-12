#include "/usr/include/bfd.h"
#include <unistd.h>
#include <dlfcn.h>
#include <stdlib.h>
#include "libobjdata.h"
#include <string.h>
#include <math.h>

#define RDTSC(var)                                              \
  {                                                             \
    uint32_t var##_lo, var##_hi;                                \
    asm volatile("lfence\n\trdtsc" : "=a"(var##_lo), "=d"(var##_hi));\   
    var = var##_hi;                                             \
    var <<= 32;                                                 \
    var |= var##_lo;                                            \
  }

void flt_to_str(float num) {
    char *dec[] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"};
    float input = num;
    int zeroes = floor(log10(input))*-1.0;
    char s[100] = "0.";
    
    for (int i = 0; i<zeroes-1; i++) {
        strcat(s, "0");
    }
    //grabs the non-zero numbers in the float and put them in an int
    int int_input = input*round(pow(10, zeroes+2));
    int digits = floor(log10(int_input)) + 1;
    int power = digits - 1;
    while (power > 0) {
        if (int_input == 0) {
            strcat(s, "");
        }
        int expo_test = round(pow(10, power));
        if (int_input % expo_test == 0) {
            int quot = (int)(int_input / expo_test);
            strcat(s,dec[quot]);
            int_input = 0;
        } else {
            int rem = int_input % expo_test;
            int diff = int_input - rem;
            int quot = (int)(diff/expo_test);
            strcat(s,dec[quot]);
            int_input = rem;
        }
        power = power - 1;
    }
    if (int_input > 0) {
        strcat(s, dec[(int) int_input]);
    } else {
        strcat(s, "0");
    }
    write(1,s,100);
    write(1, "\n", 1);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        write(2, "Wrong arg size", 14);
        return -1;
    }
    int flag = 0; 
    char *filename = "";
    if (strcmp(argv[1],"RTLD_NOW")==0 || strcmp(argv[2],"RTLD_NOW")==0) {
        if (strcmp(argv[1],"RTLD_NOW")==0){
            filename = argv[2];
        } else {
            filename = argv[1];
        }
        write(1, "getcopy_dl for file:", 15);
        write(1,filename, strlen(filename));
        write(1, "\nRTLD_NOW Avg(seconds): ", 25);
        flag = RTLD_NOW;
    } else if(strcmp(argv[1],"RTLD_LAZY")==0 || strcmp(argv[2],"RTLD_LAZY")==0) {
        if (strcmp(argv[1],"RTLD_LAZY")==0){
            filename = argv[2];
        } else {
            filename = argv[1];
        }
        write(1, "getcopy_dl for file:", 15);
        write(1,filename, strlen(filename));
        write(1, "\nRTLD_LAZY Avg(seconds): ", 26);
        flag = RTLD_LAZY;
    } else {
        write(2, "Wrong inputs specified", 23);
        return -1;
    }
    unsigned long long int start, finish;
    void * libObjdata;
    int (*objcopy_get_copy)(const char*);
    int total = 0;
    //49 runs here for testing, need to not close 50th to actually use it
    for (int i=0; i < 49; i++){
        RDTSC(start);
            libObjdata = dlopen("libobjdata.so", flag); 
        RDTSC(finish);
        int diff = finish-start;
        //printf("finish %llu and start %llu and diff %i", finish, start, diff);
        total = total+ diff;
        //printf("total %i\n", total);
        dlclose(libObjdata);
    }
    RDTSC(start);
            libObjdata = dlopen("libobjdata.so", flag); 
    RDTSC(finish);
    int diff = finish-start;
    //printf("finish %llu and start %llu and diff %i", finish, start, diff);
    total = total+ diff;
    //printf("total %i\n", total);
    //printf("final total %i\n", total);
    float avg = (float) total / 50.0;
    float time = (float)(avg)/ 2400000000.0;
    flt_to_str(time);
    *(void**)(&objcopy_get_copy) = dlsym(libObjdata, "get_copy");
    int copy = objcopy_get_copy(argv[1]);
    if (copy == -1) {
        write(2, "error with getting copy", 24);
        return -1;
    } else {
        return 0;
    }
}