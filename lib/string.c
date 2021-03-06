// #include <config.h>
// #include <common.h>
#include <string.h>
#include <asm/byteorder.h>

/*
 * Convert a string to lowercase.
 */
void downcase(char *str)
{
	while (*str != '\0') {
		TOLOWER(*str);
		str++;
	}
}

int strncmp(const char * cs,const char * ct,size_t count)
{
        register signed char __res = 0;

        while (count) {
                if ((__res = *cs - *ct++) != 0 || !*cs++)
                        break;
                count--;
        }

        return __res;
}

char * strcpy(char * dest,const char *src)
{
        char *tmp = dest;

        while ((*dest++ = *src++) != '\0')
                /* nothing */;
        return tmp;
}

int strcmp(const char * cs,const char * ct)
{
        register signed char __res;

        while (1) {
                if ((__res = *cs - *ct++) != 0 || !*cs++)
                        break;
        }

        return __res;
}

void* s_memcpy (void * dest,const void *src,size_t count)
{
        char *tmp = (char *) dest, *s = (char *) src;

        while (count--)
                *tmp++ = *s++;

        return dest;
}

void* memcpy (void* dest, const void* src, size_t count)
{
    u32* pDest = (u32*)dest;
    u32* pSrc = (u32*)src;
    // Source and Dest 4 bytes Address Aligned ?
    if(((int)pDest % 4) || ((int)pSrc % 4)){
        return s_memcpy(dest, src, count);
    }
    // Assembly optimized ArmV7 memcpy function
    fmemcpy(dest, src, count);
    return dest;
}__attribute__((optimize("O0")));


/*
 * Get the first occurence of a directory delimiter ('/' or '\') in a string.
 * Return index into string if found, -1 otherwise.
 */
int dirdelim(char *str)
{
	char *start = str;

	while (*str != '\0') {
		if (ISDIRDELIM(*str)) return str - start;
		str++;
	}
	return -1;
}

int strlen(const char* str)
{
  const char *s;
  for (s = str; *s; ++s); /* 1 */
  return(s - str); /* 2 */
}

/* Strip whitespace chars off end of given string, in place. Return s. */
char* rstrip(char* s)
{
    char* p = s + strlen(s);
    while (p > s && isspace(*--p))
        *p = '\0';
    return s;
}

/* Return pointer to first non-whitespace char in given string. */
char* lskip(const char* s)
{
    while (*s && isspace(*s))
        s++;
    return (char*)s;
}

char *strncpy(char *dest, const char *src, int max_size)
{
   char *save = dest;
   while((*dest++ = *src++) && max_size--);
   return save;
}

/* Version of strncpy that ensures dest (size bytes) is null-terminated. */
char* strncpy0(char* dest, const char* src, int size)
{
    strncpy(dest, src, size);
    dest[size - 1] = '\0';
    return dest;
}

char* strchr(const char * s, int c)
{
	for(; *s != (char) c; ++s)
		if (*s == '\0')
			return NULL;
	return (char *) s;
}

