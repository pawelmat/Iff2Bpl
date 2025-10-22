#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

uint32_t be32(const unsigned char *b){return (b[0]<<24)|(b[1]<<16)|(b[2]<<8)|b[3];}
int main(int argc,char**argv){if(argc<2){fprintf(stderr,"usage: print_form_header file\n");return 1;}FILE*f=fopen(argv[1],"rb");if(!f){perror("fopen");return 1;}unsigned char h[12];if(fread(h,1,12,f)!=12){fprintf(stderr,"file too small\n");fclose(f);return 1;}printf("Bytes 0..11: ");for(int i=0;i<12;i++)printf("%02X ",h[i]);printf("\n");printf("FORM id: %.4s\n",h);uint32_t formsize=be32(h+4);printf("FORM size (BE): %u\n",formsize);fseek(f,0,SEEK_END);long sz=ftell(f);printf("File size: %ld\n",sz);if(sz>=8)printf("File size - 8 = %ld\n",sz-8);fclose(f);return 0;}
