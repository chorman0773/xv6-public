#include "types.h"
#include "user.h"
#include "mystdio.h"
#include "fcntl.h"

void* memcpy(void* restrict begin,const void* restrict end,size_t len){
    char* b1 = (char*)begin;
    const char* b2 = (const char*)end;
    for(;len>0;len--)
        *b1++ = *b2++;
    return begin;
}

enum {
    IO_BUF_STATUS_FOPEN = 0xeaea,
    IO_BUF_STATUS_STDSTRM = 0xcdcd,
    IO_BUF_STATUS_ERR = 0x10000,
    IO_BUF_STATUS_EOF = 0x20000,
};

char __stdin_buf[1024];
char __stdout_buf[1024];

struct _IO_buf __stdin = {
    .__status = IO_BUF_STATUS_STDSTRM,
    .__fd = 0,
    .__blen = 1024,
    .__buf = __stdin_buf,
    .__btype = _IOFBF,
    .__bufpos = 0,
};
struct _IO_buf __stdout = {
    .__status = IO_BUF_STATUS_STDSTRM,
    .__fd = 1,
    .__blen = 1024,
    .__buf = __stdout_buf,
    .__btype = _IOLBF,
    .__bufpos = 0,
};
struct _IO_buf __stderr = {
    .__status = IO_BUF_STATUS_STDSTRM,
    .__fd = 2,
    .__blen = 0,
    .__buf = NULL,
    .__btype = _IONBF,
    .__bufpos = 0,
};

int myfflush(FILE* __fp){
    if(__fp->__buf){
        int ret = write(__fp->__fd,__fp->__buf,__fp->__bufpos);
        __fp->__bufpos = 0;
        if(ret<0){
            __fp->__status |= IO_BUF_STATUS_ERR;
            return ret;
        }else{
            return 0;
        }
    }else{
        return 0;
    }
}

int myfclose(FILE* __fp){
    int flush = myfflush(__fp);
    if(flush)
        return flush;
    switch (__fp->__status&0xffff){
        case 0xeaea:
        {
            if((__fp->__btype&0x10000)!=0)
                free(__fp->__buf);
            int ret = close(__fp->__fd);
            if(ret<0)
                return ret;
            free(__fp);
            return 0;
        }
        case 0xcdcd:
            return myfflush(__fp);
        default:
            return -1; // Invalid stream, cannot close
    }
}

size_t myfwrite(const void* __buf,size_t __size,size_t __count,FILE* __fp){
    if((__fp->__btype&0xFFFF)==_IONBF){
        int res = write(__fp->__fd,__buf,__size*__count);
        if(res<0){
            __fp->__status |= IO_BUF_STATUS_ERR;
            return EOF;
        }
        return res/__size;
    }else /*if((__fp->__btype&0xFFFF)==_IOFBF)*/{
        size_t rsize = __size*__count;
        size_t written = 0;

        while(rsize>=(__fp->__blen-__fp->__bufpos)){
            size_t buflen = (__fp->__blen-__fp->__bufpos);
            __fp->__bufpos = 0;
            memcpy(__fp->__buf+__fp->__bufpos,__buf,buflen);
            __buf-=buflen;
            rsize-=buflen;
            written+=buflen;
            if(write(__fp->__fd,__fp->__buf,__fp->__blen)<=0){
                __fp->__status |= IO_BUF_STATUS_ERR;
                return EOF;
            }
        }
        written += rsize;
        memcpy(__fp->__buf+__fp->__bufpos,__buf,rsize);
        __fp->__bufpos+=rsize;
        return written/__size;
    }
}

FILE* myfopen(const char* name, const char* md){
    if(*md=='r'){
        FILE* f = malloc(sizeof(FILE));
        f->__fd = open(name,O_RDONLY);
        if(f->__fd<0){
            free(f);
            return NULL;
        }
        f->__status = IO_BUF_STATUS_FOPEN;
        f->__btype = _IONBF;
        return f;
    }else if(*md=='w'){
        FILE* f = malloc(sizeof(FILE));
        f->__fd = open(name,O_CREATE|O_WRONLY);
        if(f->__fd<0){
            free(f);
            return NULL;
        }
        f->__status = IO_BUF_STATUS_FOPEN;
        f->__btype = 0x10000|_IOFBF;
        f->__buf = malloc(1024);
        f->__blen = 1024;
        f->__bufpos = 0;
        return f;
    }else{
        return NULL;
    }
}