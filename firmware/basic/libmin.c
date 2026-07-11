/* mini bibliotheque C pour l'edition de liens freestanding */
#include "string.h"
void *memcpy(void *d, const void *s, size_t n)
{ unsigned char *a=d; const unsigned char *b=s; while(n--) *a++=*b++; return d; }
void *memmove(void *d, const void *s, size_t n)
{ unsigned char *a=d; const unsigned char *b=s;
  if (a<b) { while(n--) *a++=*b++; } else { a+=n; b+=n; while(n--) *--a=*--b; } return d; }
void *memset(void *d, int c, size_t n)
{ unsigned char *a=d; while(n--) *a++=(unsigned char)c; return d; }
int memcmp(const void *x, const void *y, size_t n)
{ const unsigned char *a=x,*b=y; while(n--){ if(*a!=*b) return *a-*b; a++;b++; } return 0; }
char *strchr(const char *s, int c)
{ for(;;s++){ if(*s==(char)c) return (char*)s; if(!*s) return 0; } }
size_t strlen(const char *s){ const char *p=s; while(*p) p++; return p-s; }
int strcmp(const char *a, const char *b)
{ while(*a && *a==*b){a++;b++;} return (unsigned char)*a-(unsigned char)*b; }
char *strcpy(char *d, const char *s){ char *r=d; while((*d++=*s++)); return r; }
