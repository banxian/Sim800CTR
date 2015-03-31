/*---------------------------------------------------------------------------*
  Project:  Horizon
  File:     FsSampleSecureSave.cpp

  Copyright (C)2009-2012 Nintendo Co., Ltd.  All rights reserved.

  These coded instructions, statements, and computer programs contain
  proprietary information of Nintendo of America Inc. and/or Nintendo
  Company Ltd., and are protected by Federal copyright law. They may
  not be disclosed to third parties or copied or duplicated in any form,
  in whole or in part, without the prior written consent of Nintendo.

  $Rev: 47440 $
 *---------------------------------------------------------------------------*/

#include <nn.h>
#include <nn/util.h>
#include <nn/os.h>
#include <string.h>
#include <wchar.h>
#include <stdlib.h>
#include "demo.h"

#include "../Common/FsSampleCommon.h"
#include "SaveDataExporter.h"

using namespace nn;

/*
 * Overview
 *
 Because download applications and other applications write save data to an SD card, users can easily rollback save data by simply backing up the data on their PC.
 * 
 *
 * This sample illustrates a method to detect this type of rollback.
 *
 * The A button prevents rollback the current state while saving.
 * The Y button reads save data and checks, at this time, if rollback has been done and displays those results.
 * 
 */

/*
 * Basic Flow
 *
 * By using the SetSaveDataSecureValue and VerifySaveDataSecureValue functions, 64-bit values can be written to and read from the system NAND memory.
 * 
 *
 * In a normal sequence, the same value is saved to save data and the system NAND memory.
 * If data is rolled back, save data values will be affected, but values saved in system NAND memory will not.
 * 
 * By determining if these values match, you can detect whether rollback has been performed.
 */

namespace sample { namespace fs { namespace sim800ctr {

// unnamed
namespace
{
    const char SAVE_DATA_ARCHIVE[] = "data:";
} // Namespace

char g_LowerTexts[0x1000] = {0, };
char g_UpperTexts[0x1000] = {0, };
nn::os::Mutex g_LogMute;

void screenlog(bool upper, const char* format, ...)
{
    //return;
    va_list va;
    va_start(va, format);
    char localbuf[0x100];
    s32 len = nn::nstd::TVSNPrintf(localbuf, sizeof(localbuf), format, va);
    if (len > 0) {
        g_LogMute.Lock();
        if (upper) {
            strcat(g_UpperTexts, localbuf);
        } else {
            strcat(g_LowerTexts, localbuf);
        }
        g_LogMute.Unlock();
    }
}

//void hardlog(const char* format, ...)
//{
//    nn::fs::FileOutputStream logfile;

//    nn::Result r = nn::fs::TryCreateFile(L"sdmc:/sde.txt", 0);
//    r = logfile.TryInitialize(L"sdmc:/sde.txt", true);
//    bool logok = r.IsSuccess();

//    if (logok) {
//        va_list va;
//        va_start(va, format);
//        char localbuf[0x100];
//        s32 len = nn::nstd::TVSNPrintf(localbuf, sizeof(localbuf), format, va);
//        if (len > 0) {
//            s32 writed;
//            logfile.TrySeek(0, nn::fs::POSITION_BASE_END);
//            logfile.TryWrite(&writed, localbuf, len, true);
//        }
//    }
//}


}}}
