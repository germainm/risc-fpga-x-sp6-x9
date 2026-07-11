/* shim string.h pour compilation freestanding de ff.c */
#ifndef STRING_SHIM_H
#define STRING_SHIM_H
#include <stddef.h>
void *memcpy(void *d, const void *s, size_t n);
void *memmove(void *d, const void *s, size_t n);
void *memset(void *d, int c, size_t n);
int   memcmp(const void *a, const void *b, size_t n);
char *strchr(const char *s, int c);
size_t strlen(const char *s);
int   strcmp(const char *a, const char *b);
char *strcpy(char *d, const char *s);
#endif
