#include "log.hpp"

#include <cstdarg>
#include <cstdio>

#ifndef PSGI_HOST
    #include <orbis/libkernel.h>
#endif

namespace psgi {

void klog(const char *fmt, ...)
{
    char buf[512];
    va_list ap; 
    va_start(ap, fmt);

    int n = vsnprintf(buf, sizeof(buf) - 1, fmt, ap);
    va_end(ap);

    if (n < 0)
    {
        const char* fallback = "klog formatting error\n";
        sceKernelDebugOutText(0, fallback);
        return;
    }

    if (n >= (int)(sizeof(buf) - 1))
        n = sizeof(buf) - 2;

    buf[n] = '\n';
    buf[n + 1] = '\0';
    
#ifdef PSGI_HOST
    fputs(buf, stdout);
    fflush(stdout);
#else
    sceKernelDebugOutText(0, buf);
#endif
}

} // namespace psgi
