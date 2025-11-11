// src/util.c
#define _POSIX_C_SOURCE 200809L
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

void uuid4(char out[37]) {
    unsigned char r[16];
    FILE* f = fopen("/dev/urandom","rb");
    if (f) { fread(r,1,16,f); fclose(f); }
    else { srand((unsigned)time(NULL)); for(int i=0;i<16;i++) r[i]=(unsigned char)(rand()&0xFF); }
    r[6] = (r[6]&0x0F)|0x40; r[8]=(r[8]&0x3F)|0x80;
    snprintf(out,37,"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        r[0],r[1],r[2],r[3],r[4],r[5],r[6],r[7],r[8],r[9],r[10],r[11],r[12],r[13],r[14],r[15]);
}
