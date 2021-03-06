#include "NekoDriver.h"
#include "DBCentre.h"
extern "C" {
#ifdef HANDYPSP
#include "ANSI/w65c02.h"
#else
#include "ANSI/65C02.h"
#endif
}
#include "CC800IOName.h"
#include "AddonFuncUnt.h" // GetTickCount
//#include <unistd.h> // usleep
#include <stdio.h>
#include <string.h> // memcpy


TScreenBuffer renderLCDBuffer;

// Storage
PNekoDriver theNekoDriver;

TNekoDriver::TNekoDriver()
    : fEmulatorThread(NULL)
    , fNorBuffer(NULL)
    , fBROMBuffer(NULL)
    , fFlashUpdated(false)
{
    // Do initialization that must be repeated for a restart
    restart = 0;

    hardlog("alloc fBROMBuffer\n");
    fBROMBuffer = (char*)malloc(512 * 0x8000); // 32K * 256 * 2
    hardlog("fBROMBuffer: 0x%08X\n", fBROMBuffer);
    for (int i = 0; i < 256; i++) {
        volume0array[i] = (unsigned char*)fBROMBuffer + i * 0x8000;
        volume1array[i] = volume0array[i] + 256 * 0x8000;
    }

    hardlog("alloc fNorBuffer\n");
    fNorBuffer = (char*)malloc(16 * 0x8000); // 32K * 16
    hardlog("fNorBuffer: 0x%08X\n", fNorBuffer);
    for (int i = 0; i < 16; i++) {
        norbankheader[i] = (unsigned char*)fNorBuffer + i * 0x8000;
    }

    // DebugInitialize();
    hardlog("MemInitialize\n");
    MemInitialize();
    // TermInitialize();
    // FrameCreateWindow();

    // enter the main message loop
    // EnterMessageLoop();

    // fEmulatorThread->start();
    hardlog("TNekoDriver()--\n");
}


TNekoDriver::~TNekoDriver()
{
    if (fNorBuffer) {
        free(fNorBuffer);
        fNorBuffer = NULL;
    }
    if (fBROMBuffer) {
        free(fBROMBuffer);
        fBROMBuffer = NULL;
    }
}


//void TNekoDriver::onLCDBufferChanged( QByteArray* buffer )
//{
//    emit lcdBufferChanged(buffer);
//}

bool TNekoDriver::StartEmulation()
{
    fEmulatorThread = new EmulatorThread(NULL, NULL);
    //connect(fEmulatorThread, SIGNAL(lcdBufferChanged(QByteArray*)), 
    //    this, SLOT(onLCDBufferChanged(QByteArray*)));
    //fEmulatorThread->start(QThread::InheritPriority);
    fEmulatorThread->start();
    return true;
}

bool TNekoDriver::StopEmulation()
{
    if (fEmulatorThread) {
        fEmulatorThread->StopKeeping();
        //fEmulatorThread->wait(4000);
        //fEmulatorThread->deleteLater();
        fEmulatorThread = NULL;
        if (fFlashUpdated) {
            SaveFullNorFlash();
        }
    }
    return true;
}

bool TNekoDriver::RunDemoBin( const std::string& filename )
{
    if (filename.empty()) {
        //LoadDemoNor(QApplication::applicationDirPath() + "/mario.bin");
        hardlog("LoadBROM\n");
        LoadBROM("rom:/obj.bin");
        hardlog("LoadFullNorFlash\n");
#ifdef CARD2
        nn::fs::FileInputStream savefls;
        nn::Result r = savefls.TryInitialize(L"data:/cc800.fls");
#else
        nn::fs::FileInputStream sdmcfls;
        nn::Result r = sdmcfls.TryInitialize(L"sdmc:/cc800.fls");
#endif
        if (r.IsFailure() && nn::fs::ResultNotFound::Includes(r)) {
            LoadFullNorFlash("rom:/cc800.fls");
        } else {
#ifdef CARD2
            LoadFullNorFlash("data:/cc800.fls");
#else
            LoadFullNorFlash("sdmc:/cc800.fls");
#endif
        }
        //fixedram0000[io00_bank_switch] = 2;
        //SwitchNorBank(2);
        //*(unsigned short*)&(pmemmap[mapE000][0x1FFC]) = 0x4018; // mario.bin
    } else {
        LoadDemoNor(filename);
        fixedram0000[io00_bank_switch] = 1;
        SwitchNorBank(1);
        *(unsigned short*)&(pmemmap[mapE000][0x1FFC]) = 0x4018; // mario.bin
    }
    //fEmulatorThread->start(QThread::InheritPriority);
    hardlog("StopEmulation\n");
    StopEmulation();
    hardlog("StartEmulation\n");
    StartEmulation();
    hardlog("RunDemoBin--\n");
    return true;
}

bool TNekoDriver::PauseEmulation()
{
    return false;
}

bool TNekoDriver::ResumeEmulation()
{
    return false;
}



EmulatorThread::EmulatorThread( char* brom, char* nor )
    : fBROMBuffer(brom)
    , fNorBuffer(nor)
    , fKeeping(true)
    , fLCDBuffer(malloc(160*80/8))
    , lastTicket(0)
    , totalcycle(0)
    , measured(false)
#ifdef AUTOTEST
    , remeasure(0)
#else
    , remeasure(2)
#endif
    , batchlimiter(0)
    , batchcount(UINT_MAX)
    , sleepgap(10)
    , sleepcount(0)
{

}

EmulatorThread::~EmulatorThread()
{
    free(fLCDBuffer);
}


extern WORD LogDisassembly (WORD offset, char* text);
extern void AppendLog(const char* text);

extern bool timer0started;
extern bool timer1started;
extern unsigned short gThreadFlags;


#if !TARGET_IPHONE_SIMULATOR
#define spdc1016freq 3686400
#endif


// WQXSIM
extern bool timer0waveoutstart;
extern int prevtimer0value;
int gDeadlockCounter = 0;
extern bool lcdoffshift0flag;
bool matrixupdated = false;
long nmicount = 0;
long twohznmicycle;

void Turnoff2HzNMIMaskAddIRQFlag();
void CheckTimebaseAndEnableIRQnEXIE1();
void EnableWatchDogFlag();
void CheckLCDOffShift0AndEnableWatchDog();

void EmulatorThread::run()
{
    // Load PC from Reset Vector
    CpuInitialize();
    lcdoffshift0flag = false;
    //stp = 1; // test
#ifndef FAKENMI
    unsigned int nmistart = GetTickCount();
#endif
    gThreadFlags &= 0xFFFEu; // Remove 0x01 from gThreadFlags (stack related)
#ifdef AUTOTEST
    unsigned totalline = 0;
    enablelogging = false;
#endif
    while(fKeeping) {
#if TARGET_IPHONE_SIMULATOR
        const unsigned spdc1016freq = GlobalSetting.SPDC1016Frequency;
#endif
#ifdef FAMENMI
        twohznmicycle = spdc1016freq / 2;
#endif
        while (batchcount >= 0 && fKeeping) {
#ifdef AUTOTEST
            totalline++;
            TryTest(totalline);
#endif // AUTOTEST
#ifdef LOGASM
#ifdef AUTOTEST
            if (enablelogging) {
#endif
#ifdef HANDYPSP
                LogDisassembly(mPC, NULL);
#else
                LogDisassembly(regs.pc, NULL);
#endif // HANDYPSP
#ifdef AUTOTEST
            }
#endif // AUTOTEST
#endif // LOGASM
            if (matrixupdated) {
                matrixupdated = false;
                AppendLog("keypadmatrix updated.");
            }

            //nmicount++; // MERGEASM
            // 2Hz NMI
            // TODO: use batchcount as NMI source
#ifdef FAKENMI
            //if (nmicount % 400000 == 0) {
            //    nmicount = 0; // MERGEASM
            if (twohznmicycle < 0) {
                twohznmicycle = spdc1016freq / 2; // reset
#else
            // in CC800 hardware, NMI is generated by 32.768k crystal, but spdc1016 use RC oscillator so cycle/NMI can be mismatched.
            unsigned int dummynow = GetTickCount();
            if (dummynow - nmistart >= 500) {
                nmistart += 500;
#endif
                //nmi = 0; // next CpuExecute will execute two instructions
                gThreadFlags |= 0x08; // Add NMIFlag
            }

            // NMI > IRQ
            if ((gThreadFlags & 0x08) != 0) {
                gThreadFlags &= 0xFFF7u; // remove 0x08 NMI Flag
                // FIXME: NO MORE REVERSE
                g_nmi = TRUE; // next CpuExecute will execute two instructions
                qDebug("ggv wanna NMI.");
                //fprintf(stderr, "ggv wanna NMI.\n");
                gDeadlockCounter--; // wrong behavior of wqxsim
#ifdef HANDYPSP
            } else if (((PS() & AF_INTERRUPT) == 0) && ((gThreadFlags & TF_IRQFLAG) != 0)) {
#else
            } else if (((regs.ps & 0x4) == 0) && ((gThreadFlags & 0x10) != 0)) {
#endif
                gThreadFlags &= 0xFFEFu; // remove 0x10 IRQ Flag
                g_irq = TRUE; // B flag (AF_BREAK) will remove in CpuExecute
                qDebug("ggv wanna IRQ.");
                gDeadlockCounter--; // wrong behavior of wqxsim
            }

            DWORD CpuTicks = CpuExecute();
            totalcycle += CpuTicks;
            twohznmicycle -= CpuTicks;
            // add checks for reset, IRQ, NMI, and other pin signals
            if (lastTicket == 0) {
                lastTicket = GetTickCount();
            }

            gDeadlockCounter++;
            if (gDeadlockCounter == 6000) {
                // overflowed
                gDeadlockCounter = 0;
                if ((gThreadFlags & 0x80u) == 0) {
                    // CheckTimerbaseAndEnableIRQnEXIE1
                    CheckTimebaseAndEnableIRQnEXIE1();
                    if (timer0started) {
                        // mayDestAddr == 5 in ReadRegister
                        // mayBaseTable have 0x100 elements?
                        //increment = mayBasetable[mayDestAddr];  // mayBaseTable[5] == 3
                        //t0overflow = (unsigned int)(increment + mayPreviousTimer0Value) < 0xFF;
                        //mayPreviousTimer0Value += increment;
                        //if ( !t0overflow )
                        //{
                        //    mayPreviousTimer0Value = 0;
                        //    Turnoff2HzNMIMaskAddIRQFlag();
                        //}
                        int increment = 3;
                        prevtimer0value += increment;
                        if (prevtimer0value >= 0xFFu) {
                            prevtimer0value = 0;
                            Turnoff2HzNMIMaskAddIRQFlag();
                        }
                    }
                } else {
                    // RESET
                    fixedram0000[io01_int_enable] |= 0x1; // TIMER A INTERRUPT ENABLE
                    fixedram0000[io02_timer0_val] |= 0x1; // [io01+1] Timer0 bit1 = 1
                    gThreadFlags &= 0xFF7F;      // remove 0x80 | 0x10
#ifdef HANDYPSP
                    mPC = *(unsigned short*)&pmemmap[mapE000][0x1FFC];
#else
                    regs.pc = *(unsigned short*)&pmemmap[mapE000][0x1FFC];
#endif
                }
            } else {
                if (timer0started) {
                    // mayDestAddr == 5 in ReadRegister
                    // mayBaseTable have 0x100 elements?
                    int increment = 3;
                    prevtimer0value += increment;
                    if (prevtimer0value >= 0xFFu) {
                        prevtimer0value = 0;
                        Turnoff2HzNMIMaskAddIRQFlag();
                    }
                }
            }
            

            //if (timer0started) {
            //    fixedram0000[io02_timer0_val] = fixedram0000[io02_timer0_val] + 1;
            //}
            //if (timer1started) {
            //    fixedram0000[io03_timer1_val] = fixedram0000[io03_timer1_val] + 1;
            //}

            // TODO: dynamic re-measure
            if (measured == false && totalcycle % spdc1016freq < 10 && totalcycle > spdc1016freq) {
                measured = true;
#if !TARGET_IPHONE_SIMULATOR
                // fixed rate on device
                // realworld time = 106
#ifdef HANDYPSP
                batchlimiter = spdc1016freq / 88; // 12*10=120ms
#else
                batchlimiter = spdc1016freq / 4;
#endif
                batchcount = batchlimiter;
#else
                if (totalcycle < spdc1016freq * 2) {
                    // first loop check!
                    // spdc1016 executed one second in fullspeed virtual timeline
                    unsigned long long realworldtime = GetTickCount() - lastTicket; // should less than 1000ms
                    lastTicket = GetTickCount();
                    //double virtual100ms = realworldtime / 100.0;
                    qDebug("realworldtime:%llu", realworldtime);
                    fprintf(stderr, "realworldtime:%llu\n", realworldtime);
                    if (realworldtime > 1000) {
                        // TODO: device may slower than simulator
                        // in my test iPad I get 3528/3779/3630 msec to finish one sdpc1016freq loop
                        // we should make screen refresh at least twice per real world second or screen will never been updated
                        // 1000->500 2000->250 4000->125
                        batchlimiter = 500 * spdc1016freq / realworldtime;
                        if (remeasure) {
                            qDebug("remeasure on batchlimiter: %u", batchlimiter);
                            fprintf(stderr, "remeasure on batchlimiter: %u\n", batchlimiter);
                            measured = false;
                            totalcycle = 0;
                            remeasure--;
                        }
                        batchcount = batchlimiter;
                    } else if (batchlimiter == 0) {
                        // 1000 - realworldtime = overflow time, overflow time / 10 = sleepcount, freq / sleepcount = batchcount
                        //batchlimiter = spdc1016freq / ((1000 - realworldtime) / 10);
                        sleepcount = (1000 - realworldtime) / sleepgap;
                        batchlimiter = spdc1016freq * sleepgap / (1000 - realworldtime);
                    } else {
                        // wrong path?
                        // sleep(0) is less than 10ms, but we'd never go here
                    }
                    batchcount = batchlimiter;
                } else {
                    // totalcycle > spdc1016freq * 2
                    // TODO:  check once more
                }
#endif // TARGET_IPHONE_SIMULATOR
            } // measured == false && totalcycle % spdc1016freq < 10 && totalcycle > spdc1016freq 
            if (totalcycle % spdc1016freq > 10 && totalcycle > spdc1016freq) {
                // FIXME: bug on slow device
                //measured = false;
            }

            if (batchlimiter != 0) {
                batchcount -= CpuTicks;
            }

            //usleep(10);
            //Sleep(0);
        }

        if (memcmp(&fixedram0000[0x9C0], fLCDBuffer, 160*80/8) != 0) {
            memcpy(fLCDBuffer, &fixedram0000[0x9C0], 160*80/8);
            qDebug("lcdBufferChanged");
            fprintf(stderr, "lcdBufferChanged\n");
            //emit lcdBufferChanged(new QByteArray((const char*)fLCDBuffer, 160*80/8));
            memcpy(renderLCDBuffer.fPixel, fLCDBuffer, 160*80/8);
        }

        Sleep(10); // SleepGap. 10ms = 10us
        if (batchlimiter > 0) {
            batchcount = batchlimiter;
        } else {
            batchcount = spdc1016freq * 2; // dirty fix
        }
    }
    //this->deleteLater();
}

void EmulatorThread::StopKeeping()
{
    fKeeping = false;
    // need in stop
    fThread.Join();
    fThread.Finalize();
}

#ifdef AUTOTEST
void EmulatorThread::TryTest( unsigned line )
{
    // Network
    if (line == 1024000) {
        keypadmatrix[1][6] = 1;
        CheckLCDOffShift0AndEnableWatchDog();
    }
    if (line == 1064000) {
        keypadmatrix[1][6] = 0;
        CheckLCDOffShift0AndEnableWatchDog();
    }
    // Down
    if (line == 1224000) {
        keypadmatrix[6][3] = 1;
        CheckLCDOffShift0AndEnableWatchDog();
    }
    if (line == 1264000) {
        keypadmatrix[6][3] = 0;
        CheckLCDOffShift0AndEnableWatchDog();
    }
    // Enter
    if (line == 1424000) {
        keypadmatrix[6][5] = 1;
        CheckLCDOffShift0AndEnableWatchDog();
        enablelogging = true;
    }
    if (line == 1524000) {
        keypadmatrix[6][5] = 0;
        CheckLCDOffShift0AndEnableWatchDog();
    }
    // Splash
    if (line == 4724000) {
        keypadmatrix[6][5] = 1;
        CheckLCDOffShift0AndEnableWatchDog();
    }
    if (line == 4764000) {
        keypadmatrix[6][5] = 0;
        CheckLCDOffShift0AndEnableWatchDog();
    }

}
#endif

void EmulatorThread::start()
{
    fThread.StartUsingAutoStack(
        EmulatorThreadFunc,
        this,
        4 * 1024, 
        nn::os::Thread::GetCurrentPriority() + 1
        );
}

void CheckLCDOffShift0AndEnableWatchDog()
{
    //// may check hotkey press
    //if ( gLcdoffShift0Flag )
    //{
    //    if ( keypadmatrix1[6] || keypadmatrix1[7] )
    //    {
    //        // Line6 Dict Card
    //        // Line7 on/off
    //        EnableWatchDogFlag();               // 0x80
    //        gLcdoffShift0Flag = 0;
    //    }
    //}
    //else
    //{
    //    // Set lcdoffshift0flag only when keypadmatrix3 == 0xFB
    //    if ( keypadmatrix1[7] == 0xFBu )
    //        gLcdoffShift0Flag = 1;
    //}
    matrixupdated = true;
    if (lcdoffshift0flag) {
        // we don't have invert bit for row6,7
        //bool row67down = false;
        for (int y = 0; y < 2; y++) {
            for (int x = 0; x < 8; x++) {
                if (keypadmatrix[y][x] == 1) {
                    //row6,7down = true;
                    EnableWatchDogFlag();
                    lcdoffshift0flag = false;
                    return;
                }
            }
        }
    } else {
        if (keypadmatrix[0][2] == 1) {
            lcdoffshift0flag = true; // this flag is used for UI? (IO05_ClockControl)
        }
    }
}

void CheckTimebaseAndEnableIRQnEXIE1()
{
    //if ( gFixedRAM0_04_general_ctrl & 0xF )
    //{
    //    // TimeBase Clock Select bit0~3
    //    LOBYTE(gThreadFlags) = gThreadFlags | 0x10; // add 0x10 to gThreadFlags
    //    gFixedRAM0[(unsigned __int8)io01_int_ctrl] |= 8u;// EXTERNAL INTERRUPT SELECT1
    //}
    if (fixedram0000[io04_general_ctrl] & 0x0F) {
        gThreadFlags |= 0x10; // Add IRQ flag
        //irq = 0; // TODO: move to NMI check
        fixedram0000[io01_int_enable] |= 0x8; // EXTERNAL INTERRUPT SELECT1
    }
}

void Turnoff2HzNMIMaskAddIRQFlag()
{
    if ( fixedram0000[io04_general_ctrl] & 0xF )
    {
        gThreadFlags |= 0x10u; // Add 0x10 Flag to gThreadFlag
        //irq = 0; // TODO: move to 
        fixedram0000[io01_int_enable] |= 0x10u; // 2Hz NMI Mask off
    }
}

void EnableWatchDogFlag()
{
    gThreadFlags |= 0x80;
}

void EmulatorThreadFunc( EmulatorThread* thiz )
{
    thiz->run();
}
