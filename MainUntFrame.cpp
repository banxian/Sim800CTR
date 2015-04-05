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
#include <nn/gx.h>
#include <nn/fs.h>

#include <vector>
#include <string>
#include "demo.h"

#include "FsSampleCommon.h"
#include "MainUntFrameHelper.h"
#include "KeyItem.h"
#include "NekoDriver.h"
#include "AddonFuncUnt.h"
#include <string.h>
#include <stdlib.h>


//extern "C" unsigned char _ZN41_GLOBAL__N__17_hid_PadReader_cpp_f13c76ec16s_IsEnableSelectE;
//extern "C" unsigned * _ZN2nn3hid3CTR21IsSelectButtonEnabledEv;

extern "C" unsigned _ZN2nn2os37_GLOBAL__N__13_os_Memory_cpp_512daea910s_HeapSizeE;

extern void CheckLCDOffShift0AndEnableWatchDog();

extern "C" unsigned char sim800stripe[];

using namespace nn;

typedef struct tagLCDStripe {
    QRect bitmap;
    int left, top;
} TLCDStripe;

namespace sample { namespace fs { namespace sim800ctr {

namespace
{
    //bool s_IsWorkerThreadAlive;
    //nn::os::Event s_WorkerEvent;
    //nn::os::Thread s_WorkerThread;
    TKeyItem* fKeyItems[8][8];
    nn::hid::TouchPanelReader m_TouchPanelReader;
    int fLastTouch;
    int fLastDownX, fLastDownY;
    GLuint fStripeID;
    TLCDStripe fLCDStripes[80];
    QRect fLCDPixel, fLCDEmpty;
    int attachindex = 0;
    std::set<int> fFrontAttaches;
} // namespace

void initKeypad();
void initLcdStripe();
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
        vec.erase(vec.begin(), vec.begin() + vec.size() - 7);
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

#ifdef NN_BUILD_VERBOSE
    attachindex++;
    if (attachindex >= 80) {
        attachindex = 0;
    }
    screenlog(true, "%d\n", attachindex);
#endif

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
                if (item->release(padStatus.release)) {
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
    if (!m_TouchPanelReader.ReadLatest(&tpStatus)) {
        return;
    }

    if (tpStatus.x && tpStatus.y) {
        //screenlog(true, "touch: %d, x: %d, y:%d\n", tpStatus.touch, tpStatus.x, tpStatus.y);
        //hardlog("touch: %d, x: %d, y:%d\n", tpStatus.touch, tpStatus.x, tpStatus.y);
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
    } else if (fLastTouch && fLastDownX && fLastDownY) {
        // only mouse up
        for (int y = 0; y < 8; y++) {
            for (int x = 0; x < 8; x++) {
                TKeyItem* item = fKeyItems[y][x];
                if (item && item->inRect(QPoint(fLastDownX, fLastDownY)) && item->pressed()) {
                    item->release();
                    hitted = true;
                }
            }
        }
    }
    //} else {
    //    // mouse up
    //    for (int y = 0; y < 8; y++) {
    //        for (int x = 0; x < 8; x++) {
    //            TKeyItem* item = fKeyItems[y][x];
    //            if (item && item->inRect(QPoint(tpStatus.x, tpStatus.y)) && item->pressed()) {
    //                item->release();
    //                hitted = true;
    //            }
    //        }
    //    }
    //}
    if (hitted) {
        repaintKeypad();
        updateKeypadMatrix();
    }
    fLastTouch = tpStatus.touch;
    fLastDownX = tpStatus.x;
    fLastDownY = tpStatus.y;
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
        new TKeyItem(51, "Shift", nn::hid::BUTTON_L),   // P01, P12
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
        new TKeyItem(47, "PgUp", nn::hid::BUTTON_ZL),   // P07, P13

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
        new TKeyItem(49, "PgDn", nn::hid::BUTTON_ZR), // P06, P16
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
    renderSystem.SetClearColor(NN_GX_DISPLAY1, 0.0f, 0.0f, 0.0f, 1.0f);
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

void FillTexturedRectangleEx(demo::RenderSystemDrawing &renderSystem, const GLuint textureId,
                                                const f32 windowCoordinateX, const f32 windowCoordinateY,
                                                const f32 rectangleWidth, const f32 rectangleHeight,
                                                const f32 imageWidth, const f32 imageHeight,
                                                const f32 textureX, const f32 textureY,
                                                const f32 textureWidth, const f32 textureHeight)
{
    renderSystem.CheckRenderTarget();

    if ( (textureId == 0 ) || (! renderSystem.HasTexture(textureId) ) )
    {        
        NN_TPANIC_("Invalid textureId %d.\n", textureId);
        return;
    }

    f32 windowCoordinateX1 = windowCoordinateX + rectangleWidth;
    f32 windowCoordinateY1 = windowCoordinateY + rectangleHeight;

    f32 texcoordS0 = textureX / textureWidth;
    f32 texcoordT0 = textureY / textureHeight;
    f32 texcoordS1 = texcoordS0 + static_cast<f32>(imageWidth) / static_cast<f32>(textureWidth);
    f32 texcoordT1 = texcoordT0 + static_cast<f32>(imageHeight) / static_cast<f32>(textureHeight);

    renderSystem.FillTexturedTriangle(textureId,
        windowCoordinateX, windowCoordinateY,
        texcoordS0, texcoordT1,
        windowCoordinateX, windowCoordinateY1,
        texcoordS0, texcoordT0,
        windowCoordinateX1, windowCoordinateY1,
        texcoordS1, texcoordT0);

    renderSystem.FillTexturedTriangle(textureId,
        windowCoordinateX, windowCoordinateY,
        texcoordS0, texcoordT1,
        windowCoordinateX1, windowCoordinateY1,
        texcoordS1, texcoordT0,
        windowCoordinateX1, windowCoordinateY,
        texcoordS1, texcoordT1);
}


void repaintLCD( demo::RenderSystemDrawing &renderSystem )
{
#ifdef NN_BUILD_VERBOSE
    renderSystem.SetColor(0.8, 0.4, 0.2); // red
    renderSystem.DrawLine(0,0,0,240);
    renderSystem.DrawLine(0,162,400,162);
#endif
    // Draw empty
    if (lcdoffshift0flag) {
        //painter.setOpacity(0.8);
        //painter.fillRect(via.rect(), QColor(0xFFFFFDE8));
        //FillTexturedRectangleEx(renderSystem, fStripeID, 0, 0, 384, 162, 384, 162, 2, 92, 512, 256);
        renderSystem.SetColor(1, 0.9921, 0.9098, 0.1);
        renderSystem.FillRectangle(0,0,384,162);
        //DrawShadowOrPixel(&renderLCDBuffer, renderSystem, false);
    } else {
        // s_RenderSystem.FillTexturedRectangle(s_BmpTexture0Id,
        //windowCoordinateX, windowCoordinateY,
         //   rectangleWidth, rectangleHeight,
         //   s_Bmp0Width, s_Bmp0Height,
         //   s_Texture0Width, s_Texture0Height);
        //renderSystem.FillTexturedRectangle(fStripeID, 0, 0, 384, 162, 384, 162, 20, 40, 512, 256);
        FillTexturedRectangleEx(renderSystem, fStripeID, 0, 0, 384, 162, 384, 162, 2, 92, 512, 256);
        //renderSystem.SetColor(1, 0.9921, 0.9098);
        //renderSystem.FillRectangle(0,0,384,162);
        //DrawShadowOrPixel(buffer, painter, true);
        //DrawShadowOrPixel(&renderLCDBuffer, renderSystem, true);
        DrawShadowOrPixel(&renderLCDBuffer, renderSystem, false);
    }

}

void DrawShadowOrPixel( TScreenBuffer* buffer, demo::RenderSystemDrawing &render, bool semishadow )
{
    int offx = 0;
    int offy = 0;
    if (semishadow) {
        offx = 2;
        offy = 2;
        render.SetColor(50/255.0, 40/255.0, 74/255.0, 0.3);
    } else {
        render.SetColor(50/255.0, 40/255.0, 74/255.0);
    }
    render.SetPointSize(2);
    if (semishadow == false) {
        // TODO: optmize by software merge
        for (int y = 79; y >= 0; y--) {
            if (fFrontAttaches.find(y) == fFrontAttaches.end()) {
                //continue;
                char pixel = buffer->fPixel[(160/8) * y];
                if ((pixel < 0)) {
                    TLCDStripe* item = &fLCDStripes[y];
                    FillTexturedRectangleEx(render, fStripeID, item->left, item->top, item->bitmap.w, item->bitmap.h, item->bitmap.w, item->bitmap.h, item->bitmap.x, 256 - item->bitmap.y - item->bitmap.h, 512, 256);
                }
            }
        }
        for (int y = 79; y >= 0; y--) {
            if (fFrontAttaches.find(y) != fFrontAttaches.end()) {
                char pixel = buffer->fPixel[(160/8) * y];
                if ((pixel < 0)) {
                    TLCDStripe* item = &fLCDStripes[y];
                    FillTexturedRectangleEx(render, fStripeID, item->left, item->top, item->bitmap.w, item->bitmap.h, item->bitmap.w, item->bitmap.h, item->bitmap.x, 256 - item->bitmap.y - item->bitmap.h, 512, 256);
                }
            }
        }
    }

    int index = 0;
    int yp = 1 + offy;
    for (int y = 0; y < 80; y++) {
        int xp = 47 - 2 - 2 + offx;
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

void initLcdStripe()
{
    QRect PageUp = QRect(48, 236, 7, 8); // lcd_pgup.png
    QRect Star = QRect(35, 175, 5, 5); // lcd_star.png
    QRect Num = QRect(8, 166, 19, 10); // lcd_num.png
    QRect Eng = QRect(2, 192, 19, 10); // lcd_eng.png
    QRect Caps = QRect(2, 204, 19, 10); // lcd_caps.png
    QRect Shift = QRect(2, 180, 19, 10); // lcd_shift.png
    QRect Sound = QRect(34, 226, 9, 8); // lcd_sound.png
    QRect Flash = QRect(48, 246, 7, 8); // lcd_flash.png
    QRect SharpBell = QRect(22, 226, 10, 8); // lcd_sharpbell.png
    QRect KeyClick = QRect(2, 228, 6, 10); // lcd_keyclick.png
    QRect Alarm = QRect(25, 239, 10, 8); // lcd_alarm.png
    QRect Speaker = QRect(23, 187, 8, 7); // lcd_speaker.png
    QRect Tape = QRect(36, 249, 10, 5); // lcd_tape.png
    QRect Minus = QRect(18, 250, 16, 4); // lcd_minus.png
    QRect Battery = QRect(2, 216, 8, 10); // lcd_battery.png
    QRect Secret = QRect(23, 205, 10, 6); // lcd_secret.png
    QRect PageLeft = QRect(45, 226, 9, 8); // lcd_pgleft.png
    QRect PageRight = QRect(37, 236, 9, 8); // lcd_pgright.png
    QRect PageDown = QRect(12, 216, 8, 9); // lcd_pgdown.png
    QRect Left = QRect(2, 240, 11, 9); // lcd_left.png
    QRect HonzFrame = QRect(22, 216, 30, 8); // lcd_hframe.png
    QRect Microphone = QRect(15, 239, 8, 9); // lcd_microphone.png
    QRect HonzBar = QRect(36, 186, 6, 4); // lcd_hbar.png
    QRect Right = QRect(10, 228, 10, 9); // lcd_right.png
    QRect SevenVert1 = QRect(35, 195, 2, 5); // lcd_seven_vert1.png
    QRect SevenVert2 = QRect(45, 166, 2, 5); // lcd_seven_vert2.png
    QRect SevenVert3 = QRect(42, 173, 2, 5); // lcd_seven_vert3.png
    QRect SevenVert4 = QRect(41, 166, 2, 5); // lcd_seven_vert4.png
    QRect SevenHonz1 = QRect(35, 182, 6, 2); // lcd_seven_honz1.png
    QRect SevenHonz2 = QRect(10, 251, 6, 2); // lcd_seven_honz2.png
    QRect SevenHonz3 = QRect(2, 251, 6, 2); // lcd_seven_honz3.png
    QRect VertFrame = QRect(388, 2, 9, 132); // lcd_vframe.png
    QRect Up = QRect(23, 196, 4, 7); // lcd_up.png
    QRect Down = QRect(29, 196, 4, 6); // lcd_down.png
    QRect Line = QRect(29, 166, 10, 7); // lcd_line.png
    QRect Line5 = QRect(23, 178, 10, 7); // lcd_line5.png
    QRect VertBar = QRect(2, 166, 4, 12); // lcd_vbar.png
    QRect SemiColon = QRect(33, 187, 1, 6); // lcd_semicolon.png
    QRect Point = QRect(22, 236, 2, 1); // lcd_point.png
    fLCDPixel = QRect(43,180,2,2);
    fLCDEmpty = QRect(2,2,384,162);

    //fLCDStripes = new TLCDStripe[80];

    // 7 segment display
    fLCDStripes[2].bitmap = SevenVert1;
    fLCDStripes[19].bitmap = SevenVert2;
    fLCDStripes[0].bitmap = SevenVert3;
    fLCDStripes[22].bitmap = SevenVert4;
    fLCDStripes[1].bitmap = SevenHonz1;
    fLCDStripes[3].bitmap = SevenHonz2;
    fLCDStripes[21].bitmap = SevenHonz3;
    int xdelta = 8;
    fLCDStripes[2].left  = 6;
    fLCDStripes[2].top   = 1;
    fLCDStripes[33].left = 6;
    fLCDStripes[33].top  = 6;
    fLCDStripes[0].left  = 2;
    fLCDStripes[0].top   = 1;
    fLCDStripes[35].left = 2;
    fLCDStripes[35].top  = 6;
    fLCDStripes[1].left  = 2;
    fLCDStripes[1].top   = 1;
    fLCDStripes[3].left  = 2;
    fLCDStripes[3].top   = 5;
    fLCDStripes[34].left = 2;
    fLCDStripes[34].top  = 9;

    fLCDStripes[7].bitmap = SevenVert1;
    fLCDStripes[24].bitmap = SevenVert2;
    fLCDStripes[5].bitmap = SevenVert3;
    fLCDStripes[26].bitmap = SevenVert4;
    fLCDStripes[6].bitmap = SevenHonz1;
    fLCDStripes[8].bitmap = SevenHonz2;
    fLCDStripes[25].bitmap = SevenHonz3;
    fLCDStripes[7].left  = fLCDStripes[2].left + xdelta;
    fLCDStripes[7].top   = fLCDStripes[2].top;
    fLCDStripes[29].left = fLCDStripes[33].left + xdelta;
    fLCDStripes[29].top  = fLCDStripes[33].top;
    fLCDStripes[5].left  = fLCDStripes[0].left + xdelta;
    fLCDStripes[5].top   = fLCDStripes[0].top;
    fLCDStripes[31].left = fLCDStripes[35].left + xdelta;
    fLCDStripes[31].top  = fLCDStripes[35].top;
    fLCDStripes[6].left  = fLCDStripes[1].left + xdelta;
    fLCDStripes[6].top   = fLCDStripes[1].top;
    fLCDStripes[8].left  = fLCDStripes[3].left + xdelta;
    fLCDStripes[8].top   = fLCDStripes[3].top;
    fLCDStripes[30].left = fLCDStripes[34].left + xdelta;
    fLCDStripes[30].top  = fLCDStripes[34].top;

    fLCDStripes[13].bitmap = SevenVert1;
    fLCDStripes[29].bitmap = SevenVert2;
    fLCDStripes[10].bitmap = SevenVert3;
    fLCDStripes[31].bitmap = SevenVert4;
    fLCDStripes[11].bitmap = SevenHonz1;
    fLCDStripes[14].bitmap = SevenHonz2;
    fLCDStripes[30].bitmap = SevenHonz3;
    xdelta += 1;
    fLCDStripes[13].left = fLCDStripes[7].left  + xdelta;
    fLCDStripes[13].top  = fLCDStripes[7].top;
    fLCDStripes[24].left = fLCDStripes[29].left + xdelta;
    fLCDStripes[24].top  = fLCDStripes[29].top;
    fLCDStripes[10].left = fLCDStripes[5].left  + xdelta;
    fLCDStripes[10].top  = fLCDStripes[5].top;
    fLCDStripes[26].left = fLCDStripes[31].left + xdelta;
    fLCDStripes[26].top  = fLCDStripes[31].top;
    fLCDStripes[11].left = fLCDStripes[6].left  + xdelta;
    fLCDStripes[11].top  = fLCDStripes[6].top;
    fLCDStripes[14].left = fLCDStripes[8].left  + xdelta;
    fLCDStripes[14].top  = fLCDStripes[8].top;
    fLCDStripes[25].left = fLCDStripes[30].left + xdelta;
    fLCDStripes[25].top  = fLCDStripes[30].top;
    xdelta -= 1;

    fLCDStripes[17].bitmap = SevenVert1;
    fLCDStripes[33].bitmap = SevenVert2;
    fLCDStripes[15].bitmap = SevenVert3;
    fLCDStripes[35].bitmap = SevenVert4;
    fLCDStripes[16].bitmap = SevenHonz1;
    fLCDStripes[18].bitmap = SevenHonz2;
    fLCDStripes[34].bitmap = SevenHonz3;
    fLCDStripes[17].left = fLCDStripes[13].left + xdelta;
    fLCDStripes[17].top  = fLCDStripes[13].top;
    fLCDStripes[19].left = fLCDStripes[24].left + xdelta;
    fLCDStripes[19].top  = fLCDStripes[24].top;
    fLCDStripes[15].left = fLCDStripes[10].left + xdelta;
    fLCDStripes[15].top  = fLCDStripes[10].top;
    fLCDStripes[22].left = fLCDStripes[26].left + xdelta;
    fLCDStripes[22].top  = fLCDStripes[26].top;
    fLCDStripes[16].left = fLCDStripes[11].left + xdelta;
    fLCDStripes[16].top  = fLCDStripes[11].top;
    fLCDStripes[18].left = fLCDStripes[14].left + xdelta;
    fLCDStripes[18].top  = fLCDStripes[14].top;
    fLCDStripes[21].left = fLCDStripes[25].left + xdelta;
    fLCDStripes[21].top  = fLCDStripes[25].top;

    fLCDStripes[32].bitmap = Point;
    fLCDStripes[32].left = 8;
    fLCDStripes[32].top = 12;
    fLCDStripes[9].bitmap = SemiColon;
    fLCDStripes[9].left = 17;
    fLCDStripes[9].top = 3;
    fLCDStripes[27].bitmap = Point; // TODO: Point2
    fLCDStripes[27].left = 33 / 2;
    fLCDStripes[27].top = 12;
    fLCDStripes[23].bitmap = Point;
    fLCDStripes[23].left = 51 / 2;
    fLCDStripes[23].top = 12;

    // right lines
    fLCDStripes[4].bitmap = Line;
    fLCDStripes[12].bitmap = Line5;
    fLCDStripes[20].bitmap = Line;
    fLCDStripes[28].bitmap = Line5;
    fLCDStripes[36].bitmap = Line;
    fLCDStripes[44].bitmap = Line5;
    fLCDStripes[52].bitmap = Line;
    fLCDStripes[60].bitmap = Line5;
    fLCDStripes[68].bitmap = Line;
    fLCDStripes[70].bitmap = Right;
    fLCDStripes[74].bitmap = Line;
    fLCDStripes[4].left = 371;
    fLCDStripes[4].top = 1;
    fLCDStripes[12].left = 371;
    fLCDStripes[12].top = 17;
    fLCDStripes[20].left = 371;
    fLCDStripes[20].top = 67 / 2;
    fLCDStripes[28].left = 371;
    fLCDStripes[28].top = 99 / 2;
    fLCDStripes[36].left = 371;
    fLCDStripes[36].top = 66;
    fLCDStripes[44].left = 371;
    fLCDStripes[44].top = 82;
    fLCDStripes[52].left = 371;
    fLCDStripes[52].top = 197 / 2;
    fLCDStripes[60].left = 371;
    fLCDStripes[60].top = 229 / 2;
    fLCDStripes[68].left = 371;
    fLCDStripes[68].top = 261 / 2;
    fLCDStripes[70].left = 371;
    fLCDStripes[70].top = 140;
    fLCDStripes[74].left = 371;
    fLCDStripes[74].top = 153;


    fLCDStripes[38].bitmap = PageUp;
    fLCDStripes[38].left = 14;
    fLCDStripes[38].top = 18;
    fLCDStripes[37].bitmap = Star;
    fLCDStripes[37].left = 23;
    fLCDStripes[37].top = 21;
    fLCDStripes[39].bitmap = Num;
    fLCDStripes[39].left = 12;
    fLCDStripes[39].top = 28;
    fLCDStripes[40].bitmap = Eng;
    fLCDStripes[40].left = 12;
    fLCDStripes[40].top = 77 / 2;
    fLCDStripes[41].bitmap = Caps;
    fLCDStripes[41].left = 12;
    fLCDStripes[41].top = 99 / 2;
    fLCDStripes[42].bitmap = Shift;
    fLCDStripes[42].left = 12;
    fLCDStripes[42].top = 60;
    fLCDStripes[46].bitmap = Flash;
    fLCDStripes[46].left = 47 / 2;
    fLCDStripes[46].top = 145 / 2;
    fLCDStripes[47].bitmap = Sound;
    fLCDStripes[47].left = 12;
    fLCDStripes[47].top = 147 / 2;
    fLCDStripes[48].bitmap = KeyClick;
    fLCDStripes[48].left = 25;
    fLCDStripes[48].top = 82;
    fLCDStripes[51].bitmap = SharpBell;
    fLCDStripes[51].left = 12;
    fLCDStripes[51].top = 83;
    fLCDStripes[50].bitmap = Speaker;
    fLCDStripes[50].left = 24;
    fLCDStripes[50].top = 93;
    fLCDStripes[49].bitmap = Alarm;
    fLCDStripes[49].left = 12;
    fLCDStripes[49].top = 93;
    fLCDStripes[53].bitmap = Microphone;
    fLCDStripes[53].left = 23;
    fLCDStripes[53].top = 102;
    fLCDStripes[54].bitmap = Tape;
    fLCDStripes[54].left = 12;
    fLCDStripes[54].top = 207 / 2;
    fLCDStripes[55].bitmap = Minus;
    fLCDStripes[55].left = 24;
    fLCDStripes[55].top = 113;
    fLCDStripes[58].bitmap = Battery;
    fLCDStripes[58].left = 12;
    fLCDStripes[58].top = 118;
    fLCDStripes[59].bitmap = Secret;
    fLCDStripes[59].left = 22;
    fLCDStripes[59].top = 120;
    fLCDStripes[61].bitmap = PageLeft;
    fLCDStripes[61].left = 12;
    fLCDStripes[61].top = 261 / 2;
    fLCDStripes[62].bitmap = PageRight;
    fLCDStripes[62].left = 23;
    fLCDStripes[62].top = 261 / 2;
    fLCDStripes[63].bitmap = Left;
    fLCDStripes[63].left = 23;
    fLCDStripes[63].top = 140;
    fLCDStripes[64].bitmap = PageDown;
    fLCDStripes[64].left = 12;
    fLCDStripes[64].top = 141;

    // vertframe
    fLCDStripes[65].bitmap = VertFrame;
    fLCDStripes[65].left = 2;
    fLCDStripes[65].top = 18;
    fLCDStripes[79].bitmap = Up;
    fLCDStripes[79].left = 4;
    fLCDStripes[79].top = 20;
    fLCDStripes[43].bitmap = VertBar;
    fLCDStripes[43].left = 4;
    fLCDStripes[43].top = 31;
    fLCDStripes[45].bitmap = VertBar;
    fLCDStripes[45].left = 4;
    fLCDStripes[45].top = 43;
    fLCDStripes[56].bitmap = VertBar;
    fLCDStripes[56].left = 4;
    fLCDStripes[56].top = 55;
    fLCDStripes[78].bitmap = VertBar;
    fLCDStripes[78].left = 4;
    fLCDStripes[78].top = 67;
    fLCDStripes[77].bitmap = VertBar;
    fLCDStripes[77].left = 4;
    fLCDStripes[77].top = 79;
    fLCDStripes[57].bitmap = VertBar;
    fLCDStripes[57].left = 4;
    fLCDStripes[57].top = 91;
    fLCDStripes[76].bitmap = VertBar;
    fLCDStripes[76].left = 4;
    fLCDStripes[76].top = 103;
    fLCDStripes[75].bitmap = VertBar;
    fLCDStripes[75].left = 4;
    fLCDStripes[75].top = 115;
    fLCDStripes[73].bitmap = VertBar;
    fLCDStripes[73].left = 4;
    fLCDStripes[73].top = 127;
    fLCDStripes[66].bitmap = Down;
    fLCDStripes[66].left = 4;
    fLCDStripes[66].top = 142;
    fLCDStripes[72].bitmap = HonzFrame;
    fLCDStripes[72].left = 2;
    fLCDStripes[72].top = 153;
    fLCDStripes[67].bitmap = HonzBar;
    fLCDStripes[67].left = 9;
    fLCDStripes[67].top = 155;
    fLCDStripes[69].bitmap = HonzBar;
    fLCDStripes[69].left = 29 / 2;
    fLCDStripes[69].top = 155;
    fLCDStripes[71].bitmap = HonzBar;
    fLCDStripes[71].left = 20;
    fLCDStripes[71].top = 155;

    fFrontAttaches.insert(43);
    fFrontAttaches.insert(45);
    fFrontAttaches.insert(56);
    fFrontAttaches.insert(78);
    fFrontAttaches.insert(77);
    fFrontAttaches.insert(57);
    fFrontAttaches.insert(76);
    fFrontAttaches.insert(75);
    fFrontAttaches.insert(73);

    fFrontAttaches.insert(67);
    fFrontAttaches.insert(69);
    fFrontAttaches.insert(71);

    fFrontAttaches.insert(79);
    fFrontAttaches.insert(66);

    for (int i = 0; i < 79; i++)
    {
        if (memcmp(&fLCDStripes[i].bitmap, &SevenVert1, sizeof(QRect)) == 0 || memcmp(&fLCDStripes[i].bitmap, &SevenVert2, sizeof(QRect)) == 0 || memcmp(&fLCDStripes[i].bitmap, &SevenVert3, sizeof(QRect)) == 0 || memcmp(&fLCDStripes[i].bitmap, &SevenVert4, sizeof(QRect)) == 0) {
            fFrontAttaches.insert(i);
        }
    }
}

} // namespace sim800ctr

using namespace sim800ctr;

// Call from Demo?
void MainLoop(demo::RenderSystemDrawing &renderSystem)
{
    proceedTouch();
    ProceedHid();
    ProceedDisplay(renderSystem);

    nngxWaitVSync(NN_GX_DISPLAY_BOTH);
}

// Call from Demo.
void Initialize(demo::RenderSystemDrawing& render)
{
    nn::fs::Initialize();

    const size_t ROMFS_BUFFER_SIZE = 1024 * 64; //nn::fs::GetRomRequiredRomSize(16, 16, true);
    static char buffer[ROMFS_BUFFER_SIZE];
    NN_UTIL_PANIC_IF_FAILED(nn::fs::MountRom(16, 16, buffer, ROMFS_BUFFER_SIZE));

    nn::Result r = nn::fs::MountSdmc();

    g_LogMute.Initialize();

    nn::fnd::DateTime now = nn::fnd::DateTime::GetNow();
    hardlog("=============================\n%d/%d/%d %02d:%02d:%02d\n", now.GetYear(), now.GetMonth(), now.GetDay(), now.GetHour(), now.GetMinute(), now.GetSecond());

    //glBindTexture(GL_TEXTURE_2D, s_TextureAlpha);
    //glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 32, 32, 0, GL_RGBA, GL_UNSIGNED_BYTE, g_TexAlpha);
    //glBindTexture(GL_TEXTURE_2D, textureId);
    //glTexImage2D(target, 0, internalFormat,
    //    width, height, 0,
    //    format, type, pixels);

    hardlog("GenerateTexture\n");
    render.GenerateTexture(GL_TEXTURE_2D,
        GL_RGBA, 512, 256,
        GL_RGBA, GL_UNSIGNED_BYTE, sim800stripe, fStripeID);
    //LoadTexture0();

    initLcdStripe();

    hardlog("initKeypad\n");
    initKeypad();

    //_ZN41_GLOBAL__N__17_hid_PadReader_cpp_f13c76ec16s_IsEnableSelectE = 1;
    //_ZN2nn3hid3CTR21IsSelectButtonEnabledEv = (unsigned*)(unsigned(_ZN2nn3hid3CTR21IsSelectButtonEnabledEv) & ~1);


    //s_WorkerEvent.Initialize(false);
    //s_IsWorkerThreadAlive = true;
    //NN_ERR_THROW_FATAL_ALL(s_WorkerThread.TryStartUsingAutoStack(&WorkerThread, 4 * 1024, nn::os::Thread::GetCurrentPriority() + 1));

#ifdef CARD2
    nn::Result result = nn::fs::MountSaveData();

    if(result.IsFailure()) {
        if((result <= nn::fs::ResultNotFormatted()) ||
            (result <= nn::fs::ResultBadFormat()) ||
            (result <= nn::fs::ResultVerificationFailed()))
        {
            screenlog(true, "Ooops. need to format save data...\n");

            const size_t    maxFiles        = 8;
            const size_t    maxDirectories  = 8;
            const bool      isDuplicateAll  = true; // Duplicates the entire save data region

            result = nn::fs::FormatSaveData(maxFiles, maxDirectories, isDuplicateAll);
            if(result.IsFailure()) {
                screenlog(true, "Cannot format save data!\n");
            }
        }

        result = nn::fs::MountSaveData();
        if(result.IsFailure()) {
            screenlog(true, "2nd mount, almost failed...\n");
        }
    }
#endif

    hardlog("new TNekoDriver\n");
    theNekoDriver = new TNekoDriver(); // <- stop

    //theNekoDriver->StartEmulation();
    //theNekoDriver->StopEmulation();
    hardlog("RunDemoBin++\n");
    theNekoDriver->RunDemoBin("");
    //theNekoDriver->LoadFullNorFlash();
    hardlog("Initialize--\n");
}

// Call from Demo.
void Finalize(demo::RenderSystemDrawing &render)
{
    //s_IsWorkerThreadAlive = false;
    //s_WorkerEvent.Signal();
    //s_WorkerThread.Join();

    //SaveAppSettings();
    theNekoDriver->StopEmulation();
    //delete[] fLCDStripes;
    delete theNekoDriver;

    nn::fs::Unmount("rom:");
    nn::fs::Unmount("sdmc:");
#ifdef CARD2
    nn::fs::Unmount("data:");
#endif

    render.DeleteTexture(fStripeID);
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

