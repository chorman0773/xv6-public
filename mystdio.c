#include "mystdio.h"
#include "unistd.h"
#include "stdlib.h"
#include "fcntl.h"

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
    .__fd = STDIN_FILENO,
    .__blen = 1024,
    .__buf = &__stdin_buf,
    .__btype = _IOFBF,
    .__bufpos = 0,
};
struct _IO_buf __stdout = {
    .__status = IO_BUF_STATUS_STDSTRM,
    .__fd = STDOUT_FILENO,
    .__blen = 1024,
    .__buf = &__stdout_buf,
    .__btype = _IOLBF,
    .__bufpos = 0,
};
extern struct _IO_buf __stderr = {
    .__status = IO_BUF_STATUS_STDSTRM,
    .__fd = STDERR_FILENO,
    .__blen = 0,
    .__buf = NULL,
    .__btype = _IONBF,
    .__bufpos = 0,
};

int fflush(FILE* __fp){
    if(__fp->__buf){
        int ret = write(__fp->__fd,__fp->__buf,__fp->__bufpos);
        if(ret<0){
            __fp->__status |= IO_BUF_STATUS_ERR;
            return ret;
        }else{
            fdatasync(__fp->__fd);
            return 0;
        }
    }else{
        return 0;
    }
}

int fclose(FILE* __fp){
    int flush = fflush(__fp);
    if(flush)
        return flush;
    switch (__fp->__status&0xffff){
        case 0xeaea:
        {
            if(__fp->__btype&0x10000!=0)
                free(__fp->__buf);
            int ret = close(__fp->__fd);
            if(ret<0)
                return ret;
            free(__fp);
            return 0;
        }
        case 0xcdcd:
            return fflush(__fp);
        default:
            return -1; // Invalid stream, cannot close
    }
}


FILE* freopen(const char* __name,const char* __mode, FILE* __fp){

}