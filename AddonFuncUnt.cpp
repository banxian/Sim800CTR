#include "AddonFuncUnt.h"
#ifndef _WIN32
#include <nn.h>
#endif




int greedydiv( int n, int m )
{
    int Result = (n + m - 1) / m;
    return Result;
}

#ifndef _WIN32
void Sleep( unsigned ms )
{
    nn::os::Thread::Sleep(nn::fnd::TimeSpan::FromMicroSeconds(ms));
}

unsigned int GetTickCount()
{
    return nn::os::Tick::GetSystemCurrent().ToMicroSeconds();
}
#endif

#ifndef SHIP_BUILD
void hardlog(const char* format, ...)
{
    nn::fs::FileOutputStream logfile;

    nn::Result r = nn::fs::TryCreateFile(L"sdmc:/sim800_log.txt", 0);
    r = logfile.TryInitialize(L"sdmc:/sim800_log.txt", true);
    bool logok = r.IsSuccess();

    if (logok) {
        va_list va;
        va_start(va, format);
        char localbuf[0x100];
        s32 len = nn::nstd::TVSNPrintf(localbuf, sizeof(localbuf), format, va);
        if (len > 0) {
            s32 writed;
            logfile.TrySeek(0, nn::fs::POSITION_BASE_END);
            logfile.TryWrite(&writed, localbuf, len, true);
        }
    }
}
#endif