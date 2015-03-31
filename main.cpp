/*---------------------------------------------------------------------------*
  Project:  Horizon
  File:     main.cpp

  Copyright (C)2009-2012 Nintendo Co., Ltd.  All rights reserved.

  These coded instructions, statements, and computer programs contain
  proprietary information of Nintendo of America Inc. and/or Nintendo
  Company Ltd., and are protected by Federal copyright law.  They may
  not be disclosed to third parties or copied or duplicated in any form,
  in whole or in part, without the prior written consent of Nintendo.

  $Rev: 47435 $
 *---------------------------------------------------------------------------*/

#include <nn.h>
#include <nn/fs.h>
#include <nn/font.h>
#include <nn/math.h>
#include <nn/pl.h>
#include <nn/util.h>
#include <string.h>
#include <wchar.h>
#include <stdlib.h>

#include "demo.h"
#include "applet.h"


namespace
{
const char s_ShaderBinaryFilePath[] = "rom:/nnfont_RectDrawerShader.shbin";

nn::fnd::ExpHeap          s_AppHeap;
demo::RenderSystemDrawing s_RenderSystem;

const s32 nDeviceMemorySizeVideo = 4 * 1024 * 1024;
const s32 nDeviceMemorySizeOther = 4 * 1024 * 1024;
const s32 nDeviceMemorySize = nDeviceMemorySizeVideo + nDeviceMemorySizeOther;

bool    s_InitAsciiString   = false;
u32     s_Counter           = 0;
u8      s_Color             = 0;

wchar_t g_LowerTexts[0x1000] = {0, };
wchar_t g_UpperTexts[0x1000] = {0, };

//---------------------------------------------------------------------------
// Please see man pages for details.
//
//
//---------------------------------------------------------------------------
void*
InitShaders(nn::font::RectDrawer* pDrawer)
{
    nn::fs::FileReader shaderReader(s_ShaderBinaryFilePath);

    const u32 fileSize = (u32)shaderReader.GetSize();

    void* shaderBinary = s_AppHeap.Allocate(fileSize);
    NN_NULL_ASSERT(shaderBinary);

#ifndef NN_BUILD_RELEASE
    s32 read =
#endif // NN_BUILD_RELEASE
    shaderReader.Read(shaderBinary, fileSize);
    NN_ASSERT(read == fileSize);

    const u32 vtxBufCmdBufSize =
        nn::font::RectDrawer::GetVertexBufferCommandBufferSize(shaderBinary, fileSize);
    void *const vtxBufCmdBuf = s_AppHeap.Allocate(vtxBufCmdBufSize);
    NN_NULL_ASSERT(vtxBufCmdBuf);
    pDrawer->Initialize(vtxBufCmdBuf, shaderBinary, fileSize);

    s_AppHeap.Free(shaderBinary);

    return vtxBufCmdBuf;
}

/*---------------------------------------------------------------------------*
 Please see man pages for details.
 *---------------------------------------------------------------------------*/
void
InitGX()
{
    glClearColor(0.3f, 0.3f, 0.3f, 1.0f);
}

//---------------------------------------------------------------------------
//  Please see man pages for details.
//
//
//
//---------------------------------------------------------------------------
void
InitDraw(
    int     width,
    int     height
)
{
    // Color buffer information
    // Switches the width and height to match the LCD direction.
    const nn::font::ColorBufferInfo colBufInfo = { width, height, PICA_DATA_DEPTH24_STENCIL8_EXT };

    const u32 screenSettingCommands[] =
    {

        // Viewport settings
        NN_FONT_CMD_SET_VIEWPORT( 0, 0, colBufInfo.width, colBufInfo.height ),

        // Disable scissoring
        NN_FONT_CMD_SET_DISABLE_SCISSOR( colBufInfo ),

        // Disable W buffer
        // Set the depth range
        // Disable polygon offset
        NN_FONT_CMD_SET_WBUFFER_DEPTHRANGE_POLYGONOFFSET(
            0.0f,           // wScale : W buffer is disabled at 0.0
            0.0f,           // depth range near
            1.0f,           // depth range far
            0,              // polygon offset units : polygon offset is disabled with 0.0
            colBufInfo),
    };

    nngxAdd3DCommand(screenSettingCommands, sizeof(screenSettingCommands), true);

    static const u32 s_InitCommands[] =
    {
        // Disable culling
        NN_FONT_CMD_SET_CULL_FACE( NN_FONT_CMD_CULL_FACE_DISABLE ),

        // Disable stencil test
        NN_FONT_CMD_SET_DISABLE_STENCIL_TEST(),

        // Disable depth test
        // Make all elements of the color buffer writable
        NN_FONT_CMD_SET_DEPTH_FUNC_COLOR_MASK(
            false,  // isDepthTestEnabled
            0,      // depthFunc
            true,   // depthMask
            true,   // red
            true,   // Green
            true,   // Blue
            true),  // Alpha

        // Disable early depth test
        NN_FONT_CMD_SET_ENABLE_EARLY_DEPTH_TEST( false ),

        // Framebuffer access control
        NN_FONT_CMD_SET_FBACCESS(
            true,   // colorRead
            true,   // colorWrite
            false,  // depthRead
            false,  // depthWrite
            false,  // stencilRead
            false), // stencilWrite
    };

    nngxAdd3DCommand(s_InitCommands, sizeof(s_InitCommands), true);
}

//---------------------------------------------------------------------------
//  Please see man pages for details.
//
//
//
//
//
//---------------------------------------------------------------------------
bool
InitFont(
    nn::font::ResFont*  pFont,
    void*               pBuffer
)
{
    // Sets the font resource.
    bool bSuccess = pFont->SetResource(pBuffer);

    // Sets the render buffer.
    const u32 drawBufferSize = nn::font::ResFont::GetDrawBufferSize(pBuffer);
    void* drawBuffer = s_AppHeap.Allocate(drawBufferSize, 4);
    pFont->SetDrawBuffer(drawBuffer);
    NN_NULL_ASSERT(drawBuffer);

    return bSuccess;
}

//---------------------------------------------------------------------------
//  Please see man pages for details.
//
//
//---------------------------------------------------------------------------
void
CleanupFont(nn::font::ResFont* pFont)
{
    // Disable render buffer
    // If the render buffer is set, returns the pointer to the buffer passed to SetDrawBuffer during creation.
    // 
    void *const drawBuffer = pFont->SetDrawBuffer(NULL);
    if (drawBuffer != NULL)
    {
        s_AppHeap.Free(drawBuffer);
    }
}

//---------------------------------------------------------------------------
//  Please see man pages for details.
//
//
//
//
//---------------------------------------------------------------------------
nn::font::DispStringBuffer*
AllocDispStringBuffer(int charMax)
{
    const u32 DrawBufferSize = nn::font::CharWriter::GetDispStringBufferSize(charMax);
    void *const bufMem = s_AppHeap.Allocate(DrawBufferSize);
    NN_NULL_ASSERT(bufMem);

    return nn::font::CharWriter::InitDispStringBuffer(bufMem, charMax);
}

//---------------------------------------------------------------------------
//  Please see man pages for details.
//
//
//
//
//---------------------------------------------------------------------------
void
SetupTextCamera(
    nn::font::RectDrawer*   pDrawer,
    int                     width,
    int                     height
)
{
    // Set the projection matrix for an orthogonal projection
    {
        // Sets the origin at the upper left, and the Y and Z axes in opposite directions.
        nn::math::MTX44 proj;
        f32 znear   =  0.0f;
        f32 zfar    = -1.0f;
        f32 t       =  0;
        f32 b       =  static_cast<f32>(width);
        f32 l       =  0;
        f32 r       =  static_cast<f32>(height);
        nn::math::MTX44OrthoPivot(&proj, l, r, b, t, znear, zfar, nn::math::PIVOT_UPSIDE_TO_TOP);
        pDrawer->SetProjectionMtx(proj);
    }

    // Set the model view matrix as an identity matrix
    {
        nn::math::MTX34 mv;
        nn::math::MTX34Identity(&mv);
        pDrawer->SetViewMtxForText(mv);
    }
}

//---------------------------------------------------------------------------
//  Please see man pages for details.
//
//
//
//
//
//
//---------------------------------------------------------------------------
void
DrawAscii(
    nn::font::RectDrawer*           pDrawer,
    nn::font::DispStringBuffer*     pDrawStringBuf,
    nn::font::ResFont*              pFont,
    int                             width,
    int                             height,
    nn::pl::SharedFontType          sharedFontType
)
{
    nn::font::WideTextWriter writer;
    writer.SetDispStringBuffer(pDrawStringBuf);
    writer.SetFont(pFont);

    writer.SetCursor(0, 0);
    writer.SetScale(0.4f);

    // Create the string render command once because the string does not change.
    if (! s_InitAsciiString)
    {
        writer.StartPrint();
            //(void)writer.Print(L"DEMO: SharedFont\n");
            //(void)writer.Print(L"\n");

            // Display a sample of ASCII characters
            //(void)writer.Print(L"All ASCII Character listing:\n");
            //(void)writer.Print(L"\n");
            //(void)writer.Print(L" !\"#$%&'()*+,-./\n");
            //(void)writer.Print(L"0123456789:;<=>?\n");
            //(void)writer.Print(L"@ABCDEFGHIJKLMNO\n");
            //(void)writer.Print(L"PQRSTUVWXYZ[\\]^_\n");
            //(void)writer.Print(L"`abcdefghijklmno\n");
            //(void)writer.Print(L"pqrstuvwxyz{|}~\n");
            //(void)writer.Print(L"\n");
            switch (sharedFontType)
            {
            case nn::pl::SHARED_FONT_TYPE_STD:
                //(void)writer.Print(L"こんにちは！");
                break;
            default:
                break;
            }
            (void)writer.Print(g_UpperTexts);
        writer.EndPrint();
        pDrawer->BuildTextCommand(&writer);

        s_InitAsciiString = true;
    }

    // The character's color can be changed without regenerating the string render command.
    //writer.SetTextColor(nn::util::Color8(s_Color, 255, s_Color, 255));
    s_Color++;

    pDrawer->DrawBegin();

        SetupTextCamera(pDrawer, width, height);
        writer.UseCommandBuffer();

    pDrawer->DrawEnd();
}

//---------------------------------------------------------------------------
//  Please see man pages for details.
//
//
//
//
//
//
//---------------------------------------------------------------------------
void
DrawCounter(
    nn::font::RectDrawer*           pDrawer,
    nn::font::DispStringBuffer*     pDrawStringBuf,
    nn::font::ResFont*              pFont,
    int                             width,
    int                             height
)
{
    nn::font::WideTextWriter writer;
    writer.SetDispStringBuffer(pDrawStringBuf);
    writer.SetFont(pFont);

    writer.SetCursor(0, 0);
    writer.SetScale(0.5f);

    pDrawer->DrawBegin();

        SetupTextCamera(pDrawer, width, height);

        // Generates the counter render command each time to use it.
        //writer.StartPrint();
        //    (void)writer.Printf("Counter      : %d\n", s_Counter);
        //writer.EndPrint();
        //pDrawer->BuildTextCommand(&writer);
        //writer.UseCommandBuffer();

        // Generates a counter 1/60 render command each time to use it.
        writer.StartPrint();
            //(void)writer.Printf("Counter 1/60 : %d\n", s_Counter / 60);
            (void)writer.Print(g_LowerTexts);
        writer.EndPrint();
        pDrawer->BuildTextCommand(&writer);
        writer.UseCommandBuffer();

    pDrawer->DrawEnd();

    s_Counter++;
}

}   // Namespace


//---------------------------------------------------------------------------
//  Please see man pages for details.
//---------------------------------------------------------------------------
void StartDemo()
{
    nn::fs::Initialize();

    const size_t ROMFS_BUFFER_SIZE = 1024 * 64;
    static char buffer[ROMFS_BUFFER_SIZE];
    NN_UTIL_PANIC_IF_FAILED(
        nn::fs::MountRom(16, 16, buffer, ROMFS_BUFFER_SIZE));

    // Prepares device memory and initializes graphics
    uptr addrDeviceMemory = nn::os::GetDeviceMemoryAddress();
    s_AppHeap.Initialize(addrDeviceMemory, nDeviceMemorySize );
    uptr addrDeviceMemoryVideo = reinterpret_cast<uptr>(s_AppHeap.Allocate(nDeviceMemorySizeVideo));
    s_RenderSystem.Initialize(addrDeviceMemoryVideo, nDeviceMemorySizeVideo);
    InitGX();

    // Shared font initialization
    NN_UTIL_PANIC_IF_FAILED(nn::pl::InitializeSharedFont());

    // Wait until shared font has completed loading
    while (nn::pl::GetSharedFontLoadState() != nn::pl::SHARED_FONT_LOAD_STATE_LOADED)
    {
        // Confirm that shared font loading has not failed
        if (nn::pl::GetSharedFontLoadState() == nn::pl::SHARED_FONT_LOAD_STATE_FAILED)
        {
            NN_TPANIC_("Failed to load shared font!\n");
        }
        nn::os::Thread::Sleep(nn::fnd::TimeSpan::FromMilliSeconds(1));
    }

    // Obtain the shared font type
    nn::pl::SharedFontType sharedFontType = nn::pl::GetSharedFontType();

    // Obtain the address of the shared font data
    void* pFontBuffer = nn::pl::GetSharedFontAddress();

    // Font initialization
    nn::font::ResFont font;

    // Build font
    {
#ifndef NN_BUILD_RELEASE
        bool bSuccess =
#endif // NN_BUILD_RELEASE
        InitFont(&font, pFontBuffer);
        NN_ASSERTMSG(bSuccess, "Failed to load ResFont.");
    }

    // Build the render resource
    nn::font::RectDrawer drawer;
    void *const drawerBuf = InitShaders(&drawer);

    // Allocate buffer for render string
    nn::font::DispStringBuffer *const pDrawStringBuf0 = AllocDispStringBuffer(0x1000); // Upper
    nn::font::DispStringBuffer *const pDrawStringBuf1 = AllocDispStringBuffer(0x1000); // Lower

    // After this, the application itself handles sleep
    TransitionHandler::EnableSleep();

    // MountSaveData
    size_t pos = 0;
    //size_t upos = 0;
    nn::Result result = nn::fs::MountSaveData("data:");
    if(result.IsFailure()) {
        if((result <= nn::fs::ResultNotFormatted()) ||
           (result <= nn::fs::ResultBadFormat()) ||
           (result <= nn::fs::ResultVerificationFailed()))
        {
            pos += nnnstdTSWPrintf(&g_LowerTexts[pos], L"fuck. need to format save data...\n");

            //const size_t    maxFiles        = 8;
            //const size_t    maxDirectories  = 8;
            //const bool      isDuplicateAll  = true; // Duplicates the entire save data region

            //result = nn::fs::FormatSaveData(maxFiles, maxDirectories, isDuplicateAll);
            //if(result.IsFailure()) {
            //    pos += nnnstdTSWPrintf(&g_LowerTexts[pos], L"Cannot format save data!\n");
            //}
        }

        result = nn::fs::MountSaveData();
        if(result.IsFailure()) {
            pos += nnnstdTSWPrintf(&g_LowerTexts[pos], L"2nd mount, almost failed...\n");
        }
    }

    if(result.IsSuccess()) {

        size_t maxFiles = 8;
        size_t maxDirectories = 8;
        bool isDuplicateAll = true;
        //size_t entrylimiter = 512;
        nn::Result r = nn::fs::GetSaveDataFormatInfo(&maxFiles, &maxDirectories, &isDuplicateAll);

        if (r.IsSuccess()) {
            //entrylimiter = maxFiles + maxDirectories;
            pos += nnnstdTSWPrintf(&g_LowerTexts[pos], L"maxFiles: %d, maxDirectories: %d, isDuplicateAll: %d.\n", maxFiles, maxDirectories, isDuplicateAll);
        }

        nn::fs::Unmount("data:");

    } else {
        pos += nnnstdTSWPrintf(&g_LowerTexts[pos], L"mount failed: 0x%08X\n", result.GetPrintableBits());
    }

    pos += nnnstdTSWPrintf(&g_LowerTexts[pos], L"idle\n");

    // Display font data
    volatile bool loop = true;
    while (loop)
    {
        s_RenderSystem.SetRenderTarget(NN_GX_DISPLAY0);
        {
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            InitDraw(NN_GX_DISPLAY0_WIDTH, NN_GX_DISPLAY0_HEIGHT);

            DrawAscii(&drawer, pDrawStringBuf0, &font, NN_GX_DISPLAY0_WIDTH, NN_GX_DISPLAY0_HEIGHT, sharedFontType);
        }
        s_RenderSystem.SwapBuffers();

        s_RenderSystem.SetRenderTarget(NN_GX_DISPLAY1);
        {
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            InitDraw(NN_GX_DISPLAY1_WIDTH, NN_GX_DISPLAY1_HEIGHT);

            DrawCounter(&drawer, pDrawStringBuf1, &font, NN_GX_DISPLAY1_WIDTH, NN_GX_DISPLAY1_HEIGHT);
        }
        s_RenderSystem.SwapBuffers();

        s_RenderSystem.WaitVsync(NN_GX_DISPLAY_BOTH);

        // Run sleep, HOME Menu, and POWER Menu transitions.
        TransitionHandler::Process();
        // Determine whether there has been an exit notification.
        if (TransitionHandler::IsExitRequired())
        {
            break;
        }
    }

    drawer.Finalize();

    // Destroy render resource
    s_AppHeap.Free(drawerBuf);

    // Destroy font
    CleanupFont(&font);

    // Free the render string buffer
    s_AppHeap.Free(pDrawStringBuf1);
    s_AppHeap.Free(pDrawStringBuf0);
    s_AppHeap.Free(reinterpret_cast<void*>(addrDeviceMemoryVideo));

    s_AppHeap.Finalize();
}

//---------------------------------------------------------------------------
//  Please see man pages for details.
//---------------------------------------------------------------------------
void nnMain()
{
    NN_LOG("Demo Start\n");

    // Initialize via the applet.
    // Sleep Mode is rejected automatically until TransitionHandler::EnableSleep is called.
    TransitionHandler::Initialize();
    // Here we need to check for exit notifications.
    if (!TransitionHandler::IsExitRequired())
    {
        StartDemo();
    }
    // Finalize via the applet.
    TransitionHandler::Finalize();

    NN_LOG("Demo End\n");

    // Finalize the application. The call to nn::applet::CloseApplication does not return.
    nn::applet::CloseApplication();
}

/*---------------------------------------------------------------------------*
  End of file
 *---------------------------------------------------------------------------*/
