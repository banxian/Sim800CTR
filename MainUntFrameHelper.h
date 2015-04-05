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

extern char g_LowerTexts[0x1000];
extern char g_UpperTexts[0x1000];

extern nn::os::Mutex g_LogMute;

//void hardlog(const char* format, ...);
void screenlog(bool upper, const char* format, ...);
}}}

#endif
