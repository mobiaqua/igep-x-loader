#ifndef __STRING_H__
#define __STRING_H__

#include <config.h>
#include <common.h>

#define ISDIRDELIM(c)   ((c) == '/' || (c) == '\\')
#define TOLOWER(c)	if((c) >= 'A' && (c) <= 'Z'){(c)+=('a' - 'A');}

#define isspace(c)  (c ==  ' ' || c == '\f' || c == '\n' || c == '\r' ||\
                                c == '\t' || c == '\v')


void downcase(char *str);
int strncmp(const char * cs,const char * ct,size_t count);
char * strcpy(char * dest,const char *src);
int strcmp(const char * cs,const char * ct);
void * memcpy(void * dest,const void *src,size_t count);
int dirdelim(char *str);
int strlen(const char* str);
char* rstrip(char* s);
char* lskip(const char* s);
char *strncpy(char *dest, const char *src, int max_size);
char* strncpy0(char* dest, const char* src, int size);
char* strchr(const char * s, int c);

#endif

