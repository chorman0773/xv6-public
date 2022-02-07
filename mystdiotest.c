#include "mystdio.h"
#include "types.h"
#include "user.h"

int main(){

    myprintf("Hello from a format string. S=%s, d=%d\n","Foobar",1337);
    myfflush(stdout);
    (myprintf)("Macro supression also works");
    FILE* f;
    if((f=myfopen("foo.n","w"))){
        unsigned char zeros[256] = {0};
        myfwrite(zeros,1,256,f);
        myfclose(f);
    }

    if((f=myfopen("foo.n","r"))){
        myfprintf(stderr,"Created foo.n successfully. This is stderr so unbuffered");
        myfclose(f);
    }

    myfflush(stdout);
    // Why I have to do this is beyond me
    exit();
}