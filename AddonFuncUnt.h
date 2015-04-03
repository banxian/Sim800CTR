#ifndef _ADDON_FUNC_UNIT
#define _ADDON_FUNC_UNIT

#include <string>
#include "wintypes.h"
#include "CheatTypes.h"

#ifdef SHIP_BUILD
#define hardlog(format,...)
#else
void hardlog(const char* format, ...);
#endif

bool QRectContains(const QRect& rect, int x, int y);

int greedydiv(int n, int m);
#ifndef _WIN32
unsigned int GetTickCount();
void Sleep(unsigned ms);
#endif
#endif
