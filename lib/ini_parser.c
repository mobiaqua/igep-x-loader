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
#ifdef ENABLE_LOAD_INI_FILE
#include <string.h>

#define MAX_LINE 200
#define MAX_SECTION 50
#define MAX_NAME 50

#define NULL    0
#define EOF     0x13

typedef struct tFILE {
    const char* filename;
    int file_size;
    int file_pos;
    char file_address [IGEP_INI_FILE_MAX_SIZE];
} FILE;

static FILE* xLoader_CFG = XLOADER_CFG_FILE;

static FILE* fopen (const char* filename, int from, const char* mode)
{
    int i;
    xLoader_CFG->file_size = 0;
    xLoader_CFG->file_pos = 0;
    xLoader_CFG->filename = filename;
    for(i=0; i < IGEP_INI_FILE_MAX_SIZE; i++ ) xLoader_CFG->file_address[i] = EOF;
#ifdef IGEP00X_ENABLE_MMC_BOOT
    if(from == IGEP_MMC_BOOT)
        xLoader_CFG->file_size = file_fat_read(filename, xLoader_CFG->file_address , 0);
#endif
#ifdef IGEP00X_ENABLE_FLASH_BOOT
#ifdef IGEP00X_ENABLE_MMC_BOOT
    else
#endif
        xLoader_CFG->file_size = load_jffs2_file(filename, xLoader_CFG->file_address);
#endif
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

int fgetc (FILE* fp)
{
    return fp->file_address[fp->file_pos++];
}

// #pragma GCC push_options
// #pragma GCC optimize ("O0")
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
// #pragma GCC pop_options

/* See documentation in header file. */
int ini_parse(const char* filename, int from,
              int (*handler)(void*, const char*, const char*, const char*),
              void* user)
{
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

    char* line = malloc (MAX_LINE);
    char* section = malloc (MAX_SECTION);
    char* prev_name = malloc (MAX_NAME);

    // while (fgets(line, sizeof(line), file) != NULL) { printf("%s\n", line); };
    // return 100;

    /* Scan through file line by line */
    while (fgets(line, MAX_LINE-1, file) != NULL) {
        //printf("line: %d : %s\n", lineno, line);
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
                strncpy0(section, start + 1, MAX_SECTION-1);
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
//                printf("<%s> :: <%s>\n", start, end+1);
//                printf("<%s> :: <%s> -> name<%s> value<%s>\n", start, end + 1, name, value);
#ifdef __notdef
                end = find_char_or_comment(value, '\0');
                if (*end == ';')
                    *end = '\0';
#endif
                rstrip(value);
                /* Valid name=value pair found, call handler */
                strncpy0(prev_name, name, MAX_NAME-1);
                if (!handler(user, section, name, value) && !error)
                    error = lineno;
            }
            else if (!error) {
                /* No '=' found on name=value line */
                error = lineno;
            }
        }
    }

    free(line);
    free(section);
    free(prev_name);

    fclose(file);

    return error;
}

#endif
