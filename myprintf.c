#include "mystdio.h"
#include "wctype.h"
#include "string.h"
#include "stdbool.h"

#undef myprintf
#undef myvprintf

enum{
    left = 0x01,
    sign = 0x02,
    space = 0x04,
    alt_form = 0x08,
};

struct fmt_state{
    void* strm; // Only used by resetbuf_cb
    size_t ctotal;
    const char* look;
    char* buf;
    int bpos;
    int blen;
    int(*resetbuf_cb)(struct fmt_state*);
    va_list* ap;
    int flags;
    int width;
    int size;
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
    fmt->ctotal++;
    return OK;
}

static int convert_pad(struct fmt_state* fmt,int width){
    if(width>=fmt->pad){
        if(convert_more(fmt,width)<width)
            return -1;
        fmt->ctotal+=width;
        int ret = fmt->bpos;
        fmt->bpos += width;
        return ret;
    }
    else if((fmt->flags&left)!=0){
        // TODO: Handle left padding
        return ABORT;
    }else{
        int len = convert_more(fmt,fmt->pad);
        if(len<0)
            return ABORT;
        else if(len<fmt->pad){
            fmt->ctotal+=width;
            memset(fmt->buf,fmt->padchar,len);
            fmt->bpos = len;
            return -1;
        }
        fmt->ctotal+=fmt->pad;
        memset(fmt->buf+fmt->bpos,fmt->padchar,fmt->pad);
        fmt->bpos += fmt->pad;
        return fmt->bpos-width;
    }
}


/// Handle %<prefix>c
static int convert_char(struct fmt_state* fmt){
    switch (fmt->size) {
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

static int convert_space(struct fmt_state* fmt){
    fmt->flags |= space;
    return CONTINUE;
}

static int convert_sign(struct fmt_state* fmt){
    fmt->flags |= sign;
    return CONTINUE;
}

static int convert_alt(struct fmt_state* fmt){
    fmt->flags |= alt_form;
    return CONTINUE;
}

static int convert_left(struct fmt_state* fmt){
    fmt->flags |= left;
    return CONTINUE;
}

static int convert_width(struct fmt_state* fmt){
    if(*fmt->look=='*'){ // width is in an `int` argument
        fmt->width = va_arg(*(fmt->ap),int);
    }else{
        fmt->width = 0;
        while(fmt->look[1]>='0'||fmt->look[1]<='9'){
            fmt->width *= 10;
            fmt->width += *(fmt->look++)-'0';
        }
    }
    return CONTINUE;
}

static int convert_longdbl(struct fmt_state* fmt){
    fmt->size = 'L';
    return CONTINUE;
}


static int convert_upper_hex(struct fmt_state* fmt){
    static const char alpha[16] = "0123456789ABCDEF";
    char buf[18];
    size_t tpos = 18;
    
    switch(fmt->size){
    case 0:
    {
        unsigned p = va_arg(*(fmt->ap),unsigned);
        do{
            buf[--tpos] = alpha[p&0xF];
            p>>=4;
        }while(p);
    }
    break;
    case 0x6868:
    {
        unsigned p = (unsigned char)va_arg(*(fmt->ap),unsigned);
        do{
            buf[--tpos] = alpha[p&0xF];
            p>>=4;
        }while(p);
    }
    case 'h':
    {
        unsigned p = (unsigned short)va_arg(*(fmt->ap),unsigned);
        do{
            buf[--tpos] = alpha[p&0xF];
            p>>=4;
        }while(p);
    }
    case 'l':
    case 'z':
    case 't':
    {
        unsigned long p = (unsigned long)va_arg(*(fmt->ap),unsigned long);
        do{
            buf[--tpos] = alpha[p&0xF];
            p>>=4;
        }while(p);
    }
    case 0x6C6C:
    case 'j':
    {
        unsigned long long p = (unsigned long long)va_arg(*(fmt->ap),unsigned long long);
        do{
            buf[--tpos] = alpha[p&0xF];
            p>>=4;
        }while(p);
    }
    break;
    }
    if((fmt->flags&alt_form)!=0){
        buf[--tpos] = 'X';
        buf[--tpos] = '0';
    }
    int pos = convert_pad(fmt,18-tpos);
    if(pos<0)
        return ABORT;
    memcpy((fmt->buf+pos),buf,18-tpos);
    return OK;
}

static int convert_lower_hex(struct fmt_state* fmt){
    static const char alpha[16] = "0123456789abcef";
    char buf[18];
    size_t tpos = 18;
    
    switch(fmt->size){
    case 0:
    {
        unsigned p = va_arg(*(fmt->ap),unsigned);
        do{
            buf[--tpos] = alpha[p&0xF];
            p>>=4;
        }while(p);
    }
    break;
    case 0x6868:
    {
        unsigned p = (unsigned char)va_arg(*(fmt->ap),unsigned);
        do{
            buf[--tpos] = alpha[p&0xF];
            p>>=4;
        }while(p);
    }
    case 'h':
    {
        unsigned p = (unsigned short)va_arg(*(fmt->ap),unsigned);
        do{
            buf[--tpos] = alpha[p&0xF];
            p>>=4;
        }while(p);
    }
    case 'l':
    case 'z':
    case 't':
    {
        unsigned long p = (unsigned long)va_arg(*(fmt->ap),unsigned long);
        do{
            buf[--tpos] = alpha[p&0xF];
            p>>=4;
        }while(p);
    }
    case 0x6C6C:
    case 'j':
    {
        unsigned long long p = (unsigned long long)va_arg(*(fmt->ap),unsigned long long);
        do{
            buf[--tpos] = alpha[p&0xF];
            p>>=4;
        }while(p);
    }
    break;
    }
    if((fmt->flags&alt_form)!=0){
        buf[--tpos] = 'x';
        buf[--tpos] = '0';
    }
    int pos = convert_pad(fmt,18-tpos);
    if(pos<0)
        return ABORT;
    memcpy((fmt->buf+pos),buf,18-tpos);
    return OK;
}

static int convert_octal(struct fmt_state* fmt){
    static const char alpha[16] = "01234567";
    char buf[23];
    size_t tpos = 23;
    
    switch(fmt->size){
    case 0:
    {
        unsigned p = va_arg(*(fmt->ap),unsigned);
        do{
            buf[--tpos] = alpha[p&0x7];
            p>>=3;
        }while(p);
    }
    break;
    case 0x6868:
    {
        unsigned p = (unsigned char)va_arg(*(fmt->ap),unsigned);
        do{
            buf[--tpos] = alpha[p&0x7];
            p>>=3;
        }while(p);
    }
    case 'h':
    {
        unsigned p = (unsigned short)va_arg(*(fmt->ap),unsigned);
        do{
            buf[--tpos] = alpha[p&0x7];
            p>>=3;
        }while(p);
    }
    case 'l':
    case 'z':
    case 't':
    {
        unsigned long p = (unsigned long)va_arg(*(fmt->ap),unsigned long);
        do{
            buf[--tpos] = alpha[p&0x7];
            p>>=3;
        }while(p);
    }
    case 0x6C6C:
    case 'j':
    {
        unsigned long long p = (unsigned long long)va_arg(*(fmt->ap),unsigned long long);
        do{
            buf[--tpos] = alpha[p&0x7];
            p>>=3;
        }while(p);
    }
    break;
    }
    if((fmt->flags&alt_form)!=0){
        buf[--tpos] = '0';
    }
    int pos = convert_pad(fmt,18-tpos);
    if(pos<0)
        return ABORT;
    memcpy((fmt->buf+pos),buf,18-tpos);
    return OK;
}

static int convert_signed(struct fmt_state* fmt){
    static const char alpha[10] = "0123456789";
    char buf[21];
    size_t tpos = 21;
    switch(fmt->size){
    case 0:
    {
        int val = va_arg(*(fmt->ap),int);
        bool sign = val<0;
        unsigned rval = (val<0?-val:val);
        do{
            buf[--tpos] = alpha[rval%10];
            rval /= 10;
        } while(rval);
        if(sign)
            buf[--tpos] = '-';
        else if((fmt->flags&sign)!=0)
            buf[--tpos] = '+';
        else if((fmt->flags&space)!=0)
            buf[--tpos] = ' '; 
    }
    break;
    case 0x6868:
    {
        signed char val = (signed char)va_arg(*(fmt->ap),int);
        bool sign = val<0;
        unsigned char rval = (val<0?-val:val);
        do{
            buf[--tpos] = alpha[rval%10];
            rval /= 10;
        } while(rval);
        if(sign)
            buf[--tpos] = '-';
        else if((fmt->flags&sign)!=0)
            buf[--tpos] = '+';
        else if((fmt->flags&space)!=0)
            buf[--tpos] = ' '; 
    }
    break;
    case 'h':
    {
        short val = (short)va_arg(*(fmt->ap),int);
        bool sign = val<0;
        unsigned short rval = (val<0?-val:val);
        do{
            buf[--tpos] = alpha[rval%10];
            rval /= 10;
        } while(rval);
        if(sign)
            buf[--tpos] = '-';
        else if((fmt->flags&sign)!=0)
            buf[--tpos] = '+';
        else if((fmt->flags&space)!=0)
            buf[--tpos] = ' '; 
    }
    break;
    case 'l':
    case 'z':
    case 't':
    {
        long val = (long)va_arg(*(fmt->ap),long);
        bool sign = val<0;
        unsigned long rval = (val<0?-val:val);
        do{
            buf[--tpos] = alpha[rval%10];
            rval /= 10;
        } while(rval);
        if(sign)
            buf[--tpos] = '-';
        else if((fmt->flags&sign)!=0)
            buf[--tpos] = '+';
        else if((fmt->flags&space)!=0)
            buf[--tpos] = ' '; 
            
    }
    break;
    case 0x6C6C:
    case 'j':
    {
        long val = (long long)va_arg(*(fmt->ap),long long);
        bool sign = val<0;
        unsigned long rval = (val<0?-val:val);
        do{
            buf[--tpos] = alpha[rval%10];
            rval /= 10;
        } while(rval);
        if(sign)
            buf[--tpos] = '-';
        else if((fmt->flags&sign)!=0)
            buf[--tpos] = '+';
        else if((fmt->flags&space)!=0)
            buf[--tpos] = ' ';    
    }
    break;
    }
    int pos = convert_pad(fmt,21-tpos);
    if(pos<0)
        return ABORT;
    memcpy((fmt->buf+pos),buf,21-tpos);
    return OK;
}

static int convert_unsigned(struct fmt_state* fmt){
    static const char alpha[10] = "0123456789";
    char buf[20];
    size_t tpos = 20;
    switch(fmt->size){
    case 0:
    {
        unsigned rval = va_arg(*(fmt->ap),int);
        do{
            buf[--tpos] = alpha[rval%10];
            rval /= 10;
        } while(rval);
    }
    break;
    case 0x6868:
    {
        unsigned rval = (unsigned char)va_arg(*(fmt->ap),unsigned);
        do{
            buf[--tpos] = alpha[rval%10];
            rval /= 10;
        } while(rval);
    }
    break;
    case 'h':
    {
        unsigned rval = (unsigned short)va_arg(*(fmt->ap),unsigned);
        do{
            buf[--tpos] = alpha[rval%10];
            rval /= 10;
        } while(rval);
    }
    break;
    case 'l':
    case 'z':
    case 't':
    {
        unsigned long rval = va_arg(*(fmt->ap),unsigned long);
        do{
            buf[--tpos] = alpha[rval%10];
            rval /= 10;
        } while(rval);
            
    }
    break;
    case 0x6C6C:
    case 'j':
    {
        unsigned long long rval = va_arg(*(fmt->ap),unsigned long long);
        do{
            buf[--tpos] = alpha[rval%10];
            rval /= 10;
        } while(rval);
    }
    break;
    }
    int pos = convert_pad(fmt,21-tpos);
    if(pos<0)
        return ABORT;
    memcpy((fmt->buf+pos),buf,21-tpos);
    return OK;
}

static int convert_pointer(struct fmt_state* fmt){
    static const char alpha[16] = {"0123456789abcdef"};
    char buf[10];
    size_t tpos = 10;
    void* v = va_arg(*(fmt->ap),void*);
    if(!v){
        int pos = convert_pad(fmt,5);
        if(pos<0)
            return ABORT;
        memcpy(fmt->buf+pos,"(nil)",5);
    }else{
        unsigned long l = (unsigned long)v;
        do{
            buf[--tpos] = alpha[l&0xF];
            l>>=4;
        }while(l);
        buf[--tpos] = 'x';
        buf[--tpos] = '0';
        int pos = convert_pad(fmt,10-tpos);
        if(pos<0)
            return ABORT;
        memcpy(fmt->buf+pos,buf,10-tpos);
    }
    return OK;
}


static int convert_half(struct fmt_state* fmt){
    if(fmt->look[1]=='h'){
        fmt->look++;
        fmt->size = 0x6868;
    }else{
        fmt->size = 'h';
    }
    return CONTINUE;
}

static int convert_long(struct fmt_state* fmt){
    if(fmt->look[1]=='l'){
        fmt->look++;
        fmt->size = 0x6C6C;
    }else{
        fmt->size = 'l';
    }
    return CONTINUE;
}

static int convert_intmax(struct fmt_state* fmt){
    fmt->size = 'j';
    return CONTINUE;
}

static int convert_ptrdiff(struct fmt_state* fmt){
    fmt->size = 't';
    return CONTINUE;
}

static int convert_size(struct fmt_state* fmt){
    fmt->size = 'z';
    return CONTINUE;
}

static int convert_write(struct fmt_state* fmt){
    switch(fmt->size){
    case 0:
    {
        int* n = va_arg(*(fmt->ap),int*);
        *n = fmt->ctotal;
    }
    break;
    case 0x6868:
    {
        signed char* n = va_arg(*(fmt->ap),signed char*);
        *n = fmt->ctotal;
    }
    break;
    case 'h':
    {
        short* n = va_arg(*(fmt->ap),short*);
        *n = fmt->ctotal;
    }
    break;
    case 'l':
    case 'z':
    case 't':
    {
        long* n = va_arg(*(fmt->ap),long*);
        *n = fmt->ctotal;
    }
    case 0x6C6C:
    case 'j':
    {
        long long* n = va_arg(*(fmt->ap),long long*);
        *n = fmt->ctotal;
    }
    break;
    }
    return OK;
}

static int convert_string(struct fmt_state* fmt){
    if(fmt->size=='l'){
        return ABORT; // Not dealing with this rn
    }else{
        const char* c = va_arg(*(fmt->ap),const char*);
        size_t n = strlen(c);
        int pos = convert_pad(fmt,n);
        if(pos<0)
            return ABORT;
        memcpy(fmt->buf,c,n);
        return OK;
    }
    return OK;
}


fmt_conversion* __fmt_tbl[96] = {
    convert_space, //' '
    NULL, // '!'
    NULL, // '"'"
    convert_alt, // '#'
    NULL, // '$'
    convert_literal, // '%'
    NULL, // '&'
    NULL, // '\''
    NULL, // '('
    NULL, // ')'
    convert_width, // '*'
    convert_sign, // '+'
    NULL, // ','
    convert_left, // '-'
    NULL, // '.'
    NULL, // '/'
    convert_width, // '0'
    convert_width, // '1'
    convert_width, // '2'
    convert_width, // '3'
    convert_width, // '4'
    convert_width, // '5'
    convert_width, // '6'
    convert_width, // '7'
    convert_width, // '8'
    convert_width, // '9'
    NULL, // ':'
    NULL, // ';'
    NULL, // '<'
    NULL, // '='
    NULL, // '>'
    NULL, // '?'
    NULL, // 'A'
    NULL, // 'B'
    NULL, // 'C'
    NULL, // 'D'
    NULL, // 'E'
    NULL, // 'F'
    NULL, // 'G'
    NULL, // 'H'
    NULL, // 'I'
    NULL, // 'J'
    NULL, // 'K'
    convert_longdbl, // 'L'
    NULL, // 'M'
    NULL, // 'N'
    NULL, // 'O'
    NULL, // 'P'
    NULL, // 'Q'
    NULL, // 'R'
    NULL, // 'S'
    NULL, // 'T'
    NULL, // 'U'
    NULL, // 'V'
    NULL, // 'W'
    convert_upper_hex, // 'X'
    NULL, // 'Y'
    NULL, // 'Z'
    NULL, // '['
    NULL, // '\\'
    NULL, // ']'
    NULL, // '^'
    NULL, // '_'
    NULL, // '`'
    NULL, // 'a'
    NULL, // 'b'
    convert_char, // 'c'
    convert_signed, // 'd'
    NULL, // 'e'
    NULL, // 'f'
    NULL, // 'g'
    convert_half, // 'h'
    convert_signed, // 'i'
    convert_intmax, // 'j'
    NULL, // 'k'
    convert_long, // 'l'
    NULL, // 'm'
    convert_write, // 'n'
    convert_octal, // 'o'
    convert_pointer, // 'p'
    NULL, // 'q'
    NULL, // 'r'
    convert_string, // 's'
    convert_ptrdiff, // 't'
    convert_unsigned, // 'u'
    NULL, // 'v'
    NULL, // 'w'
    convert_lower_hex, // 'x'
    NULL, // 'y'
    convert_size, // 'z'
    NULL, // '{'
    NULL, // '|'
    NULL, // '}'
    NULL, // '~'
    NULL, // <DEL>
};

static int convert_one(struct fmt_state* fmt){
    int status;
    do{
        status = (__fmt_tbl[(*fmt->look)-' '])(fmt);
        fmt->look++;
    }while(status==CONTINUE);
    return status;
}

static int file_write_fmt_buf(struct fmt_state* fmt){
    return myfwrite(fmt->buf,1,fmt->bpos,(FILE*)fmt->strm);
}

int myvfprintf(FILE* __fp,const char* __fmt,va_list __ap){
    char ibuf[1024];
    const char* convert;
    size_t chars = 0;
    do{
        convert = strchr(__fmt,'%');
        if(convert){
            size_t tchars = myfwrite(__fmt,1,convert-__fmt,__fp);
            if(tchars==EOF)
                return EOF;
            chars += tchars;
            struct fmt_state st = {
                .ap = &__ap,
                .resetbuf_cb = file_write_fmt_buf,
                .buf = ibuf,
                .ctotal = chars,
                .look = convert+1,
            };
            if(convert_one(&st)==ABORT)
                return EOF;
            __fmt = st.look;
            chars = st.ctotal;
        }
    }while(convert);
    size_t rest = strlen(__fmt);
    size_t val = myfwrite(__fmt,1,rest,__fp);
    if(val==EOF)
        return EOF;
    return chars+val;
}

int myvprintf(const char* __fmt,va_list __ap){
    return myvfprintf(stdout,__fmt,__ap);
}

int myfprintf(FILE* __fp, const char* __fmt, ...){
    va_list ap;
    va_start(ap,__fmt);
    size_t ret = myvfprintf(__fp,__fmt,ap);
    va_end(ap);
    return ret;
}

int myprintf(const char* __fmt, ...){
    va_list ap;
    va_start(ap,__fmt);
    size_t ret = myvfprintf(stdout,__fmt,ap);
    va_end(ap);
    return ret;
}
