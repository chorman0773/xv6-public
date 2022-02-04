#ifndef __MYSTDIO_H_2022_02_03_17_24_21
#define __MYSTDIO_H_2022_02_03_17_24_21

#include "stddef.h" // NULL and size_t
#include "stdarg.h" // va_list

struct _IO_buf{
    int __status;
    int __fd;
    size_t __blen;
    int __btype;
    char* __buf;
    int __bufpos;
};

typedef struct _IO_buf FILE;

extern struct _IO_buf __stdin;
extern struct _IO_buf __stdout;
extern struct _IO_buf __stderr;

#define stdin &__stdin
#define stdout &__stdout
#define stderr &__stderr

#define EOF (-1)
#define BUFSIZE 1024

#define _IOFBF 1
#define _IOLBF 2
#define _IONBF 0

FILE* fopen(const char* __name,const char* __mode);
FILE* freopen(const char* __name,const char* __mode,FILE* __fp);
int fclose(FILE* __fp);
int fflush(FILE* __fp);

void setbuf(FILE* __fp, char* __buf);
int setvbuf(FILE* __fp,char* __buf, int __mode, size_t __size);
size_t fwrite(const void* __buf, size_t __size,size_t __count, FILE* __fp);

int myvfprintf(FILE* __fp, const char* __fmt, va_list __ap);
int myvprintf(const char* __fmt, va_list __ap);
int myfprintf(FILE* __fp, const char* __fmt, ...);
int myprintf(const char* __fmt, ...);

#define myprintf(__fmt,...) myfprintf(stdout,__fmt,__VA_ARGS__)
#define myvprintf(__fmt,__ap) myvfprintf(stdout,__fmt,__ap)

#endif /* __MYSTDIO_H_2022_02_03_17_24_21 */
