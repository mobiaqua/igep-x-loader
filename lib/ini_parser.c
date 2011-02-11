/*
 * Copyright (C) 2010 ISEE
 *
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <config.h>
#include <common.h>

#define MAX_LINE 200
#define MAX_SECTION 50
#define MAX_NAME 50

#define isspace(c)  (c ==  ' ' || c == '\f' || c == '\n' || c == '\r' ||\
                                c == '\t' || c == '\v')

#define NULL    0
#define EOF     0x13

typedef struct tFILE {
    const char* filename;
    int file_size;
    int file_pos;
    char file_address [IGEP_INI_FILE_MAX_SIZE];
} FILE;

// #define GLOBAL_XLOADER_WORK_MEMORY      0x80000000

static FILE* xLoader_CFG = XLOADER_CFG_FILE;

/* memset */
void *(memset)(void *s, int c, int n)
{
    unsigned char *us = s;
    unsigned char uc = c;
    while (n-- != 0)
        *us++ = uc;
    return s;
}

static FILE* fopen (const char* filename, int from, const char* mode)
{
    int i;
    xLoader_CFG->filename = filename;
    for(i=0; i < 16*1024; i++ ) xLoader_CFG->file_address[i] = EOF;
    // memset(xLoader_CFG->file_address, EOF, 16*1024);
    if(from == IGEP_MMC_BOOT)
        xLoader_CFG->file_size = file_fat_read(filename, xLoader_CFG->file_address , 0);
    else
        xLoader_CFG->file_size = load_jffs2_file(filename, xLoader_CFG->file_address);
    // printf("%s : file_size %d\n", filename, xLoader_CFG->file_size );
    xLoader_CFG->file_pos = 0;
    if(xLoader_CFG->file_size > 0){
        xLoader_CFG->file_address[xLoader_CFG->file_size+1] = EOF;
        return xLoader_CFG;
    }
    return NULL;
}

void fclose (FILE* fp)
{
    fp->filename = NULL;
    fp->file_pos = 0;
    fp->file_size = 0;
}

int strlen(const char* str)
{
  const char *s;
  for (s = str; *s; ++s); /* 1 */
  return(s - str); /* 2 */
}

/* Strip whitespace chars off end of given string, in place. Return s. */
static char* rstrip(char* s)
{
    char* p = s + strlen(s);
    while (p > s && isspace(*--p))
        *p = '\0';
    return s;
}

/* Return pointer to first non-whitespace char in given string. */
static char* lskip(const char* s)
{
    while (*s && isspace(*s))
        s++;
    return (char*)s;
}

/* Return pointer to first char c or ';' comment in given string, or pointer to
   null at end of string if neither found. ';' must be prefixed by a whitespace
   character to register as a comment. */
static char* find_char_or_comment(const char* s, char c)
{
    int was_whitespace = 0;
    while (*s && *s != c && !(was_whitespace && *s == ';')) {
        was_whitespace = isspace(*s);
        s++;
    }
    return (char*)s;
}

char *strncpy(char *dest, const char *src, int max_size)
{
   char *save = dest;
   while((*dest++ = *src++) && max_size--);
   return save;
}

/* Version of strncpy that ensures dest (size bytes) is null-terminated. */
static char* strncpy0(char* dest, const char* src, int size)
{
    strncpy(dest, src, size);
    dest[size - 1] = '\0';
    return dest;
}

int fgetc (FILE* fp)
{
    return fp->file_address[fp->file_pos++];
}

char* fgets(char *buf, int bsize, FILE *fp)
{
    int i;
    int c;
    int rem_r = 0;
    int done = 0;
    if (buf == 0 || bsize <= 0 || fp == 0)
        return 0;
    for (i = 0; !done && i < bsize - 1; i++) {
        c = fgetc(fp);
        if (c == EOF) {
            done = 1;
            i--;
        } else {
            if(c == '\r'){
                rem_r++;
                continue;
            }
            else {
                buf[i] = c;
                if (c == '\n'){
                    done = 1;
                    buf[i-rem_r] = '\0';
                }
            }
        }
            //else
              //  buf[i] = c;
            // done = 1;
    }
    buf[i] = '\0';
    if (i == 0)
        return 0;
    else
        return buf;
}

/* See documentation in header file. */
int ini_parse(const char* filename, int from,
              int (*handler)(void*, const char*, const char*, const char*),
              void* user)
{
    /* Uses a fair bit of stack (use heap instead if you need to) */
    char line[MAX_LINE];
    char section[MAX_SECTION] = "";
    char prev_name[MAX_NAME] = "";

    FILE* file;
    char* start;
    char* end;
    char* name;
    char* value;
    int lineno = 0;
    int error = 0;

    file = fopen(filename, from , "r");
    if (!file)
        return -1;

    /* Scan through file line by line */
    while (fgets(line, sizeof(line), file) != NULL) {
        lineno++;
        start = lskip(rstrip(line));

#if INI_ALLOW_MULTILINE
        if (*prev_name && *start && start > line) {
            /* Non-black line with leading whitespace, treat as continuation
               of previous name's value (as per Python ConfigParser). */
            if (!handler(user, section, prev_name, start) && !error)
                error = lineno;
        }
        else
#endif
        if (*start == ';' || *start == '#') {
            /* Per Python ConfigParser, allow '#' comments at start of line */
        }
        else if (*start == '[') {
            /* A "[section]" line */
            end = find_char_or_comment(start + 1, ']');
            if (*end == ']') {
                *end = '\0';
                strncpy0(section, start + 1, sizeof(section));
                *prev_name = '\0';
            }
            else if (!error) {
                /* No ']' found on section line */
                error = lineno;
            }
        }
        else if (*start && *start != ';') {
            /* Not a comment, must be a name=value pair */
            end = find_char_or_comment(start, '=');
            if (*end == '=') {
                *end = '\0';
                name = rstrip(start);
                value = lskip(end + 1);
                end = find_char_or_comment(value, '\0');
                if (*end == ';')
                    *end = '\0';
                rstrip(value);

                /* Valid name=value pair found, call handler */
                strncpy0(prev_name, name, sizeof(prev_name));
                if (!handler(user, section, name, value) && !error)
                    error = lineno;
            }
            else if (!error) {
                /* No '=' found on name=value line */
                error = lineno;
            }
        }
    }

    fclose(file);

    return error;
}
