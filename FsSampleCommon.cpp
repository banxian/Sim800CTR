/*---------------------------------------------------------------------------*
  Project:  Horizon
  File:     FsSampleCommon.cpp

  Copyright (C)2009-2012 Nintendo Co., Ltd.  All rights reserved.

  These coded instructions, statements, and computer programs contain
  proprietary information of Nintendo of America Inc. and/or Nintendo
  Company Ltd., and are protected by Federal copyright law.  They may
  not be disclosed to third parties or copied or duplicated in any form,
  in whole or in part, without the prior written consent of Nintendo.

  $Rev: 47228 $
 *---------------------------------------------------------------------------*/

#include <nn.h>
#include "demo.h"

#include "FsSampleCommon.h"

namespace sample { namespace fs {

void MainLoop(demo::RenderSystemDrawing &renderSystem);
void Initialize(demo::RenderSystemDrawing &renderSystem);
void Finalize(demo::RenderSystemDrawing &renderSystem);

}} // namespace sample::fs

using namespace sample::fs;

void nnMain()
{
    nn::applet::CTR::Enable(false);

    const f32 LINE_WIDTH    =  1.f;

    nn::fnd::ExpHeap heap;
    heap.Initialize(nn::os::GetDeviceMemoryAddress(), nn::os::GetDeviceMemorySize() );

    uptr heapForGx = reinterpret_cast<uptr>(heap.Allocate(0x800000));
    demo::RenderSystemDrawing renderSystem;
    renderSystem.Initialize(heapForGx, 0x800000);
    renderSystem.SetClearColor(NN_GX_DISPLAY_BOTH, 0.f, 0.f, 0.f, 0.f);
    renderSystem.SetFontSize(FONT_SIZE);
    renderSystem.SetLineWidth(LINE_WIDTH);
    renderSystem.SetRenderTarget(NN_GX_DISPLAY0);
    renderSystem.Clear();
    renderSystem.SwapBuffers();
    renderSystem.SetRenderTarget(NN_GX_DISPLAY1);
    renderSystem.Clear();
    renderSystem.SwapBuffers();

    NN_UTIL_PANIC_IF_FAILED(nn::hid::Initialize());

    Initialize(renderSystem);

    while (1)
    {
        MainLoop(renderSystem);

        if (nn::applet::CTR::IsExpectedToCloseApplication())
        {
            break;
        }

        if (nn::applet::CTR::IsExpectedToProcessHomeButton())
        {
            nn::applet::CTR::ProcessHomeButtonAndWait();

            if (nn::applet::CTR::IsExpectedToCloseApplication())
            {
                break;
            }

            nn::applet::CTR::ClearHomeButtonState();
            nngxUpdateState(NN_GX_STATE_ALL);
            nngxValidateState(NN_GX_STATE_ALL, GL_TRUE);
        }
    }

    Finalize(renderSystem);

    heap.Free(reinterpret_cast<void*>(heapForGx));
    heap.Finalize();

    nn::applet::CloseApplication();
}
