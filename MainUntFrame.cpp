/*---------------------------------------------------------------------------*
  Project:  Horizon
  File:     FsSampleSecureSaveFrame.cpp

  Copyright (C)2009-2012 Nintendo Co., Ltd.  All rights reserved.

  These coded instructions, statements, and computer programs contain
  proprietary information of Nintendo of America Inc. and/or Nintendo
  Company Ltd., and are protected by Federal copyright law.  They may
  not be disclosed to third parties or copied or duplicated in any form,
  in whole or in part, without the prior written consent of Nintendo.

  $Rev: 47440 $
 *---------------------------------------------------------------------------*/

#include <nn.h>
#include <nn/os.h>
#include <vector>
#include <string>
#include "demo.h"

#include "../Common/FsSampleCommon.h"
#include "SaveDataExporter.h"
#include "KeyItem.h"
#include "NekoDriver.h"
#include "AddonFuncUnt.h"

//extern "C" unsigned char _ZN41_GLOBAL__N__17_hid_PadReader_cpp_f13c76ec16s_IsEnableSelectE;
//extern "C" unsigned * _ZN2nn3hid3CTR21IsSelectButtonEnabledEv;

extern "C" unsigned _ZN2nn2os37_GLOBAL__N__13_os_Memory_cpp_512daea910s_HeapSizeE;

extern void CheckLCDOffShift0AndEnableWatchDog();


using namespace nn;

namespace sample { namespace fs { namespace sim800ctr {

namespace
{
    //enum ENUM_TASK
    //{
    //    TASK_NONE,
    //    TASK_EXPORT,
    //    TASK_IMPORT,

    //    ENUM_TASK_NUM
    //} s_Task;

    //enum ENUM_STATE
    //{
    //    STATE_NONE,
    //    STATE_DATA_FORMATTED,
    //    STATE_EXPORT,
    //    STATE_EXPORT_SUCCEDED,
    //    STATE_EXPORT_FAILED,

    //    STATE_IMPORT,
    //    STATE_IMPORT_SUCCEDED,
    //    STATE_IMPORT_FAILED,

    //    STATE_RENEW_SUCCEEDED,

    //    ENUM_STATE_NUM
    //} s_State;

    bool s_IsWorkerThreadAlive;
    nn::os::Event s_WorkerEvent;
    //nn::os::Thread s_WorkerThread;
    TKeyItem* fKeyItems[8][8];
    nn::hid::TouchPanelReader m_TouchPanelReader;
} // namespace

void initKeypad();
void repaintKeypad();
void repaintKeypad(demo::RenderSystemDrawing &renderSystem);
void repaintLCD(demo::RenderSystemDrawing &renderSystem);
void DrawShadowOrPixel( TScreenBuffer* buffer, demo::RenderSystemDrawing &painter, bool semishadow );
void onKeypadSizeChanged(int w, int h);
void updateKeypadMatrix();
void proceedTouch();


typedef std::vector<std::string> stringvec;

void DrawMultiLine(const f32 windowCoordinateX, const f32 windowCoordinateY, int increase, demo::RenderSystemDrawing &renderSystem, char* text)
{
    stringvec vec;
    while (char* line = strchr(text, '\n')) {
        std::string item;
        item.assign(text, line - text);
        vec.push_back(item);
        text = line + 1;
    }
    vec.push_back(text);

    if (vec.size() > 8) {
        vec.erase(vec.begin(), vec.begin() + vec.size() - 18);
        text[0] = 0;
    }

    f32 y = windowCoordinateY;
    for (stringvec::const_iterator it = vec.begin(); it != vec.end(); it++) {
        renderSystem.DrawText(windowCoordinateX, y, it->c_str());
        y += increase;
    }
}

void ProceedDisplay(demo::RenderSystemDrawing &renderSystem)
{
    renderSystem.SetRenderTarget(NN_GX_DISPLAY0);
    renderSystem.Clear();
    renderSystem.SetColor(1.0f, 1.0f, 1.0f); // white

    repaintLCD(renderSystem);

    g_LogMute.Lock();
    DrawMultiLine(0, 166, LINE_HEIGHT + 1, renderSystem, g_UpperTexts);
    g_LogMute.Unlock();

    renderSystem.SwapBuffers();

    renderSystem.SetRenderTarget(NN_GX_DISPLAY1);
    renderSystem.Clear();
    renderSystem.SetColor(1.0f, 1.0f, 1.0f);

    // DrawKeyPad
    repaintKeypad(renderSystem);

 
    g_LogMute.Lock();
    //renderSystem.DrawText(0, LINE_HEIGHT *  2, g_LowerTexts);
    DrawMultiLine(0, LINE_HEIGHT * 2, LINE_HEIGHT + 1, renderSystem, g_LowerTexts);
    g_LogMute.Unlock();

    renderSystem.SwapBuffers();
}

//void WorkerThread()
//{
//    while (s_IsWorkerThreadAlive)
//    {
//        s_WorkerEvent.Wait();
//        switch (s_Task)
//        {
//        case TASK_EXPORT:
//            s_State = STATE_EXPORT;
//            s_State = (ExportSaveData())? STATE_EXPORT_SUCCEDED : STATE_EXPORT_FAILED;
//            break;

//        case TASK_IMPORT:
//            s_State = STATE_IMPORT;
//            s_State = (ImportSaveData())?STATE_IMPORT_SUCCEDED:STATE_IMPORT_FAILED;
//            break;

//        //case TASK_RENEW:
//        //    s_State = STATE_RENEW_SUCCEEDED;
//        //    s_SaveData.Reset();
//        //    break;
//        default:
//            s_State = STATE_NONE;
//            break;
//        }

//        //s_DisplayParam.versionValueOnMemory = s_SaveData.version;
//        s_Task = TASK_NONE;
//    }
//}

void ProceedHid()
{
    static nn::hid::PadReader padReader;
    static nn::hid::PadStatus padStatus;

    if (!padReader.ReadLatest(&padStatus))
    {
        nn::dbg::detail::TPrintf("Getting pad status failed.\n");
    }
    if (padStatus.trigger == 0 && padStatus.release == 0) {
        return;
    }

    // trigger = all push event combine
    // release = all release event
    //screenlog(true, "trigger: 0x%08X, release: 0x%08X\n", padStatus.trigger, padStatus.release);
    //hardlog("trigger: 0x%08X, release: 0x%08X\n", padStatus.trigger, padStatus.release);
    bool hitted = false;
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            TKeyItem* item = fKeyItems[y][x];
            if (item) {
                if (item->press(padStatus.trigger)) {
                    hitted = true;
                }
                if (item->press(padStatus.release)) {
                    hitted = true;
                }
            }
        }
    }
    if (hitted) {
        repaintKeypad();
        //updateKeypadRegisters();
        updateKeypadMatrix();
    }
}

void proceedTouch()
{
    nn::hid::TouchPanelStatus tpStatus;
    if (m_TouchPanelReader.ReadLatest(&tpStatus) == false) {
        return;
    }

    bool hitted = false;
    if (tpStatus.touch) {
        // mouse down
        for (int y = 0; y < 8; y++) {
            for (int x = 0; x < 8; x++) {
                TKeyItem* item = fKeyItems[y][x];
                if (item && item->inRect(QPoint(tpStatus.x, tpStatus.y)) && item->pressed() == false) {
                    item->press();
                    hitted = true;
                }
            }
        }
    } else {
        // mouse up
        for (int y = 0; y < 8; y++) {
            for (int x = 0; x < 8; x++) {
                TKeyItem* item = fKeyItems[y][x];
                if (item && item->inRect(QPoint(tpStatus.x, tpStatus.y)) && item->pressed()) {
                    item->release();
                    hitted = true;
                }
            }
        }
    }
    if (hitted) {
        repaintKeypad();
        updateKeypadMatrix();
    }
}

void initKeypad()
{
    TKeyItem* item[8][8] = {
        NULL,       // P00, P10
        NULL,       // P01, P10
        new TKeyItem(18, "ON/OFF", nn::hid::BUTTON_START),        // P02, P10
        NULL,       // P03, P10
        NULL,       // P04, P10
        NULL,       // P05, P10
        NULL,       // P06, P10
        NULL,       // P07, P10

        new TKeyItem(0, "Dictionary", 0),          // P00, P11
        new TKeyItem(1, "Card", 0),          // P01, P11
        new TKeyItem(2, "Calculator", 0),          // P02, P11
        new TKeyItem(3, "Reminder", 0),          // P03, P11
        new TKeyItem(4, "Data", 0),          // P04, P11
        new TKeyItem(5, "Time", 0),        // P05, P11
        new TKeyItem(6, "Network", 0),        // P06, P11
        NULL,       // P07, P11

        new TKeyItem(50, "Help", nn::hid::BUTTON_R),  // P00, P12
        new TKeyItem(51, "Shift", 0),   // P01, P12
        new TKeyItem(52, "Caps", 0), // P02, P12
        new TKeyItem(53, "Esc", nn::hid::BUTTON_B),     // P03, P12
        new TKeyItem(54, "0", 0),           // P04, P12
        new TKeyItem(55, ".", 0),      // P05, P12
        new TKeyItem(56, "=", 0),       // P06, P12
        new TKeyItem(57, "Left", nn::hid::BUTTON_LEFT),     // P07, P12

        new TKeyItem(40, "Z", 0),           // P00, P13
        new TKeyItem(41, "X", 0),           // P01, P13
        new TKeyItem(42, "C", 0),           // P02, P13
        new TKeyItem(43, "V", 0),           // P03, P13
        new TKeyItem(44, "B", 0),           // P04, P13
        new TKeyItem(45, "N", 0),           // P05, P13
        new TKeyItem(46, "M", 0),           // P06, P13
        new TKeyItem(47, "PgUp", 0),   // P07, P13

        new TKeyItem(30, "A", 0),       // P00, P14
        new TKeyItem(31, "S", 0),       // P01, P14
        new TKeyItem(32, "D", 0),       // P02, P14
        new TKeyItem(33, "F", 0),       // P03, P14
        new TKeyItem(34, "G", 0),       // P04, P14
        new TKeyItem(35, "H", 0),       // P05, P14
        new TKeyItem(36, "J", 0),       // P06, P14
        new TKeyItem(37, "K", 0),       // P07, P14

        new TKeyItem(20, "Q", 0),       // P00, P15
        new TKeyItem(21, "W", 0),       // P01, P15
        new TKeyItem(22, "E", 0),       // P02, P15
        new TKeyItem(23, "R", 0),       // P03, P15
        new TKeyItem(24, "T", 0),       // P04, P15
        new TKeyItem(25, "Y", 0),       // P05, P15
        new TKeyItem(26, "U", 0),       // P06, P15
        new TKeyItem(27, "I", 0),       // P07, P15

        new TKeyItem(28, "O", 0),           // P00, P16
        new TKeyItem(38, "L", 0),           // P01, P16
        new TKeyItem(48, "Up", nn::hid::BUTTON_UP),         // P02, P16
        new TKeyItem(58, "Down", nn::hid::BUTTON_DOWN),     // P03, P16
        new TKeyItem(29, "P", 0),           // P04, P16
        new TKeyItem(39, "Enter", nn::hid::BUTTON_A),   // P05, P16
        new TKeyItem(49, "PgDn", 0), // P06, P16
        new TKeyItem(59, "Right", nn::hid::BUTTON_RIGHT),   // P07, P16

        NULL,       // P00, P17
        NULL,       // P01, P17
        new TKeyItem(12, "F1", nn::hid::BUTTON_X),       // P02, P17
        new TKeyItem(13, "F2", nn::hid::BUTTON_Y),       // P03, P17
        new TKeyItem(14, "F3", 0),       // P04, P17
        new TKeyItem(15, "F4", 0),       // P05, P17
        NULL,       // P06, P17
        NULL,       // P07, P17
    };
    // number key
    //item[3][4]->addKeycode(Qt::Key_1);
    //item[3][5]->addKeycode(Qt::Key_2);
    //item[3][6]->addKeycode(Qt::Key_3);
    //item[4][4]->addKeycode(Qt::Key_4);
    //item[4][5]->addKeycode(Qt::Key_5);
    //item[4][6]->addKeycode(Qt::Key_6);
    //item[5][4]->addKeycode(Qt::Key_7);
    //item[5][5]->addKeycode(Qt::Key_8);
    //item[5][6]->addKeycode(Qt::Key_9);

    item[3][4]->setSubscript("1");
    item[3][5]->setSubscript("2");
    item[3][6]->setSubscript("3");
    item[4][4]->setSubscript("4");
    item[4][5]->setSubscript("5");
    item[4][6]->setSubscript("6");
    item[5][4]->setSubscript("7");
    item[5][5]->setSubscript("8");
    item[5][6]->setSubscript("9");

    item[1][0]->setSubscript("C2E");
    item[1][1]->setSubscript("Note");
    item[1][2]->setSubscript("Conv");
    item[1][3]->setSubscript("Test");
    item[1][4]->setSubscript("Game");
    item[1][5]->setSubscript("Etc");

    item[2][1]->setSubscript("IME");
    item[2][3]->setSubscript("AC");

    //item[6][0]->addKeycode(Qt::Key_Slash); // O /
    //item[6][1]->addKeycode(Qt::Key_Asterisk); // L *
    //item[6][2]->addKeycode(Qt::Key_Minus); // Up -
    //item[6][3]->addKeycode(Qt::Key_Plus); // Down +

    //item[2][6]->addKeycode(Qt::Key_Space); // =
    //item[7][3]->addKeycode(Qt::Key_Backspace); // F2
    //item[6][5]->addKeycode(Qt::Key_Enter); // keyboard vs keypad

    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            fKeyItems[y][x] = item[y][x];
            if (item[y][x] == NULL) {
                keypadmatrix[y][x] = 2;
            }
        }
    }
    onKeypadSizeChanged(320, 240);
}

void repaintKeypad()
{

}

void repaintKeypad( demo::RenderSystemDrawing &renderSystem )
{
    renderSystem.SetClearColor(NN_GX_DISPLAY1, 0.2f, 0.2f, 0.2f, 1.0f);
    renderSystem.Clear();

    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            TKeyItem* item = fKeyItems[y][x];
            if (item && item->pressed() == false) {
                item->paintSelf(renderSystem);
            }
        }
    }
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            TKeyItem* item = fKeyItems[y][x];
            if (item && item->pressed()) {
                item->paintSelf(renderSystem);
            }
        }
    }

}

void updateKeypadMatrix()
{
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            TKeyItem* item = fKeyItems[y][x];
            if (item) {
                keypadmatrix[y][x] = item->pressed();
            }
        }
    }
    // TODO: Check
    // FIXME: ON/OFF detection?
    CheckLCDOffShift0AndEnableWatchDog();
    //AppendLog("keypadmatrix updated.");
}

void onKeypadSizeChanged(int w, int h)
{
    // reset keypad size
    int itemwidth = (w - 2) / 10;
    int itemheight = (h - 2) / 6;
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            TKeyItem* item = fKeyItems[y][x];
            if (item) {
                //item->setRect(QRect(x * itemwidth + 1, y * itemheight + 1, itemwidth, itemheight));
                item->setRect(QRect(item->column() * itemwidth + 1, item->row() * itemheight + 1, itemwidth, itemheight));
            }
        }
    }
    //repaintKeypad();
}

void repaintLCD( demo::RenderSystemDrawing &renderSystem )
{
    // Draw empty
    if (lcdoffshift0flag) {
        //painter.setOpacity(0.8);
        //painter.fillRect(via.rect(), QColor(0xFFFFFDE8));
    } else {
        //DrawShadowOrPixel(buffer, painter, true);
        DrawShadowOrPixel(&renderLCDBuffer, renderSystem, false);
    }

}

void DrawShadowOrPixel( TScreenBuffer* buffer, demo::RenderSystemDrawing &render, bool semishadow )
{
    if (semishadow) {
        //painter.setOpacity(0.2);
        //QTransform shadow;
        //shadow.translate(2, 2);
        //painter.setTransform(shadow, true);
    } else {
        //painter.resetTransform();
        //painter.setOpacity(1);
    }
    for (int y = 79; y >= 0; y--)
    {
        char pixel = buffer->fPixel[(160/8) * y];
        if (pixel < 0) {
            //TLCDStripe* item = &fLCDStripes[y];
            ////painter.drawImage(QRect(item->left * 2 - 2, item->top * 2 - 2, item->bitmap.width(), item->bitmap.height()), item->bitmap);
            //painter.drawImage(QRect(item->left, item->top, item->bitmap.width(), item->bitmap.height()), item->bitmap);
        }
    }

    int index = 0;
    int yp = 1;
    for (int y = 0; y < 80; y++) {
        int xp = 47 - 2 - 2;
        for (int i = 0; i < 160 / 8; i++) {
            const unsigned char pixel = buffer->fPixel[index];
            // try to shrink jump cost
            if (pixel) {
                if (i && (pixel & 0x80u)) {
                    render.DrawPoint(xp, yp);
                }
                if (pixel & 0x40) {
                    render.DrawPoint(xp + 2, yp);
                }
                if (pixel & 0x20) {
                    render.DrawPoint(xp + 4, yp);
                }
                if (pixel & 0x10) {
                    render.DrawPoint(xp + 6, yp);
                }
                if (pixel & 0x08) {
                    render.DrawPoint(xp + 8, yp);
                }
                if (pixel & 0x04) {
                    render.DrawPoint(xp + 10, yp);
                }
                if (pixel & 0x02) {
                    render.DrawPoint(xp + 12, yp);
                }
                if (pixel & 0x01) {
                    render.DrawPoint(xp + 14, yp);
                }
            }
            xp += 16;
            index++;
        }
        yp += 2;
    }
}

} // namespace sim800ctr

using namespace sim800ctr;

// Call from Demo?
void MainLoop(demo::RenderSystemDrawing &renderSystem)
{
    //proceedTouch();
    ProceedHid();
    ProceedDisplay(renderSystem);

    nngxWaitVSync(NN_GX_DISPLAY_BOTH);
}

// Call from Demo.
void Initialize(demo::RenderSystemDrawing&)
{
    //hardlog("heapsize: 0x%08X\n", _ZN2nn2os37_GLOBAL__N__13_os_Memory_cpp_512daea910s_HeapSizeE);
    //nn::init::InitializeAllocator(0x8000 * 600); // 512
    //hardlog("heapsize: 0x%08X\n", _ZN2nn2os37_GLOBAL__N__13_os_Memory_cpp_512daea910s_HeapSizeE);
    nn::fs::Initialize();

    //const size_t ROMFS_BUFFER_SIZE = 1024 * 64; //nn::fs::GetRomRequiredRomSize(16, 16, true);
    //static char buffer[ROMFS_BUFFER_SIZE];
    //NN_UTIL_PANIC_IF_FAILED(nn::fs::MountRom(16, 16, buffer, ROMFS_BUFFER_SIZE));

    nn::Result r = nn::fs::MountSdmc();

    g_LogMute.Initialize();

    //nn::fnd::DateTime now = nn::fnd::DateTime::GetNow();
    //hardlog("=============================\n%d/%d/%d %d:%d:%d\n", now.GetYear(), now.GetMonth(), now.GetDay(), now.GetHour(), now.GetMinute(), now.GetSecond());
    hardlog("=============================\n");

    hardlog("initKeypad\n");
    initKeypad();

    //_ZN41_GLOBAL__N__17_hid_PadReader_cpp_f13c76ec16s_IsEnableSelectE = 1;
    //_ZN2nn3hid3CTR21IsSelectButtonEnabledEv = (unsigned*)(unsigned(_ZN2nn3hid3CTR21IsSelectButtonEnabledEv) & ~1);


    s_WorkerEvent.Initialize(false);
    s_IsWorkerThreadAlive = true;
    //NN_ERR_THROW_FATAL_ALL(s_WorkerThread.TryStartUsingAutoStack(&WorkerThread, 4 * 1024, nn::os::Thread::GetCurrentPriority() + 1));

    //hardlog("new TNekoDriver\n");
    //theNekoDriver = new TNekoDriver(); // <- stop

    //////theNekoDriver->StartEmulation();
    //////theNekoDriver->StopEmulation();
    //hardlog("RunDemoBin++\n");
    //theNekoDriver->RunDemoBin("");
    //////theNekoDriver->LoadFullNorFlash();
    hardlog("Initialize--\n");
}

// Call from Demo.
void Finalize()
{
    s_IsWorkerThreadAlive = false;
    //s_WorkerEvent.Signal();
    //s_WorkerThread.Join();

    ////SaveAppSettings();
    //theNekoDriver->StopEmulation();
    //////delete[] fLCDStripes;
    //delete theNekoDriver;

    nn::fs::Unmount("rom:");
    nn::fs::Unmount("sdmc:");
}

}} // namespace sample::fs

extern "C" void _ZN2nn3dbg26SetDefaultExceptionHandlerEv();
extern "C" void _ZN2nn3dbg22SetDefaultBreakHandlerEv();

extern "C" void nninitStartUp(void)
{
    int appmemsize = nn::os::GetAppMemorySize();
    int usingmemsize = nn::os::GetUsingMemorySize();
    nn::os::SetupHeapForMemoryBlock(appmemsize - usingmemsize - 0x2000000);
    nn::os::SetDeviceMemorySize(0x1000000u); // 32M -> 22M
    nn::init::InitializeAllocator(0x1200000u);     // 8M -> 18M
    //`anonymous namespace'::s_UsingStartUpDefault = 1;
    nn::os::ManagedThread::InitializeEnvironment();
    //nn::dbg::SetDefaultExceptionHandler();
    //nn::dbg::SetDefaultBreakHandler();
    _ZN2nn3dbg26SetDefaultExceptionHandlerEv();
    _ZN2nn3dbg22SetDefaultBreakHandlerEv();
}