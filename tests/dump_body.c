#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

int main(int argc, char** argv) {
    if (argc < 2) { printf("usage: dump_body file\n"); return 1; }
    FILE* f = fopen(argv[1], "rb");
    if (!f) { perror("fopen"); return 1; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t* buf = malloc(sz);
    if (!buf) { perror("malloc"); fclose(f); return 1; }
    if (fread(buf,1,sz,f) != (size_t)sz) { perror("fread"); free(buf); fclose(f); return 1; }
    fclose(f);
    for (long i = 0; i < sz-7; i++) {
        if (buf[i]=='B' && buf[i+1]=='O' && buf[i+2]=='D' && buf[i+3]=='Y') {
            uint32_t size = (buf[i+4]<<24) | (buf[i+5]<<16) | (buf[i+6]<<8) | buf[i+7];
            long start = i+8;
            printf("BODY size=%u at offset %ld\n", size, start);
            for (uint32_t j=0;j<size && start+j<sz;j++) printf("%02X ", buf[start+j]);
            printf("\n");
            free(buf);
            return 0;
        }
    }
    printf("BODY not found\n");
    free(buf);
    return 1;
}
