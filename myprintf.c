#include "mystdio.h"
#include "wctype.h"
#include "string.h"

enum{
    left = 0x01,
    sign = 0x02,
    space = 0x04,
    alt_form = 0x08,
};

struct fmt_state{
    FILE* strm; // Only used by resetbuf_cb
    size_t ctotal;
    const char* look;
    char* buf;
    int bpos;
    int blen;
    int(*resetbuf_cb)(struct fmt_state*);
    va_list* ap;
    int flags;
    int width;
    int pad;
    char padchar;
};

enum{
    OK = 0,
    CONTINUE = -100,
    ABORT = -200,
};

typedef int fmt_conversion(struct fmt_state*);

#define FMT_BUF_SIZE 1024

/// Attempts to ensure that at least `chars` more narrow characters can be written to the buffer
/// This may involve writing the current buffer to the stream
///
/// Returns -1 if an error occurs, otherwise, returns the number of characters that can be written to the stream.
/// If chars is less than or equal to FMT_BUF_SIZE, then it is guaranteed that it will either fail, 
/// or leave the buffer in a state such that at least chars more narrow characters can be stored to the buffer 
static int convert_more(struct fmt_state* fmt, int chars){
    if(fmt->bpos+chars >= fmt->blen)
        if((fmt->resetbuf_cb)(fmt))
            return -1;
        
    return fmt->blen-fmt->bpos;
}

static int convert_literal(struct fmt_state* fmt){
    convert_more(fmt,1);
    fmt->buf[fmt->bpos++] = '%';
    return OK;
}

static int convert_pad(struct fmt_state* fmt,int width){
    if(width>=fmt->pad){
        if(convert_more(fmt,width)>width)
            return -1;
        int ret = fmt->bpos;
        fmt->bpos += width;
        return ret;
    }
    else if(fmt->flags&left!=0){
        // TODO: Handle left padding
        return ABORT;
    }else{
        int len = convert_more(fmt,fmt->pad);
        if(len<0)
            return ABORT;
        else if(len<fmt->pad){
            memset(fmt->buf,fmt->padchar,len);
            fmt->bpos = len;
            return -1;
        }

        memset(fmt->buf+fmt->bpos,fmt->padchar,fmt->pad);
        fmt->bpos += fmt->pad;
        return fmt->bpos-width;
    }
}


/// Handle %<prefix>c
static int convert_char(struct fmt_state* fmt){
    switch (fmt->width) {
        case 0:
            {
                int pos = convert_pad(fmt,1);
                if(pos<0)
                    return ABORT;
                fmt->buf[pos] = (char)va_arg(*(fmt->ap),int);
            }
        case 'l':
        {
            char buf[4];
            int mblen = 0;
            wint_t i = va_arg(*(fmt->ap),wint_t);
            if(i<0x80)
                buf[mblen++] = (char)i;
            else if(i<0x800){
                buf[mblen++] = (i&0x1f)|0xC0;
                buf[mblen++] = ((i>>5)&0x3f)|0x80;
            }else if(i<0x10000){
                buf[mblen++] = (i&0xf)|0xE0;
                buf[mblen++] = ((i>>4)&0x3f)|0x80;
                buf[mblen++] = ((i>>10)&0x3f)|0x80;
            }else{
                buf[mblen++] = (i&0x7)|0xF0;
                buf[mblen++] = ((i>>3)&0x3f)|0x80;
                buf[mblen++] = ((i>>9)&0x3f)|0x80;
                buf[mblen++] = ((i>>15)&0x3f)|0x80;
            }
            int pos = convert_pad(fmt,mblen);
            if(pos<0)
                return ABORT;
            memcpy(fmt->buf+pos,buf,mblen);

        }
    }
    return OK;
}