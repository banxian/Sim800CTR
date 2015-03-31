/*---------------------------------------------------------------------------*
  Project:  Horizon
  File:     FsSampleSecureSave.h

  Copyright (C)2009-2012 Nintendo Co., Ltd.  All rights reserved.

  These coded instructions, statements, and computer programs contain
  proprietary information of Nintendo of America Inc. and/or Nintendo
  Company Ltd., and are protected by Federal copyright law. They may
  not be disclosed to third parties or copied or duplicated in any form,
  in whole or in part, without the prior written consent of Nintendo.

  $Rev: 47440 $
 *---------------------------------------------------------------------------*/

#ifndef NN_SAMPLE_DEMOS_FS_SAMPLE_SECURE_SAVE_H_
#define NN_SAMPLE_DEMOS_FS_SAMPLE_SECURE_SAVE_H_

namespace sample { namespace fs { namespace sim800ctr {

// The number of save data slots used by the application
const u32   DATA_FILE_NUM       = 3;
// If a rollback prevention value is going to be used for each separate instance of save data, implementation is possible by dividing the number of instances of save data by 64 bits.
// For example, specify as follows if there are three instances of save data:
// Save data 0: 0000000000000000000000000000000000000000000*********************
// Save data 1: 0000000000000000000000*********************000000000000000000000
// Save data 2: 0*********************000000000000000000000000000000000000000000
// Here, we use rollback prevention values that are 21 bits long for each instance of save data.
const u32   SECURE_VALUE_SHIFT  = 64 / DATA_FILE_NUM;
const bit64 SECURE_VALUE_MASK = SECURE_VALUE_SHIFT == 64 ? 0xFFFFFFFFFFFFFFFFllu : ((1llu << SECURE_VALUE_SHIFT) - 1);

extern char g_LowerTexts[0x1000];
extern char g_UpperTexts[0x1000];

extern nn::os::Mutex g_LogMute;

// An example of save data used by an application
struct UserData
{
    s32 data1;
    s32 data2;
};

struct SaveData
{
    bit64    version;
    UserData userData;

    void Reset();
    void UpdateVersion();
};

//void Save(SaveData &saveData, u32 fileIndex);
//bool Load(SaveData &saveData, u32 fileIndex);
//bool InitializeSaveData();

//void hardlog(const char* format, ...);
void screenlog(bool upper, const char* format, ...);

//bool ExportSaveData();
//bool ImportSaveData();

}}}

#endif
