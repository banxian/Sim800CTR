#ifndef _NEKO_DRIVER_UNIT_H
#define _NEKO_DRIVER_UNIT_H

#include <vector>
#include <string>
#include "CheatTypes.h"
#ifndef _WIN32
#include <nn.h>
#endif

#define qDebug(...) 

class EmulatorThread;

class TNekoDriver {
public:
    TNekoDriver();
    ~TNekoDriver();
private:
    EmulatorThread* fEmulatorThread;
    char* fNorBuffer; // for performance
    char* fBROMBuffer;
    std::string fNorFilename;
    bool fFlashUpdated;
#ifndef _WIN32
    NN_PADDING3;
#endif
private:
    bool LoadBROM(const std::string& filename);
    bool LoadFullNorFlash(const std::string& filename);
    bool LoadDemoNor(const std::string& filename);
    bool SaveFullNorFlash();

public:
    bool IsProjectEmpty();
    bool IsProjectModified();
    void SwitchNorBank(int bank);
    void Switch4000toBFFF(unsigned char bank); // used 0A/0D value
    void InitInternalAddrs();
    bool StartEmulation();
    bool RunDemoBin(const std::string& filename);
    bool StopEmulation();
    bool PauseEmulation();
    bool ResumeEmulation();
    void CheckFlashProgramming(unsigned short addr, unsigned char data);

//public slots:
//    void onLCDBufferChanged(QByteArray* buffer);
//signals:
//    void lcdBufferChanged(QByteArray* buffer);
};

void EmulatorThreadFunc(EmulatorThread* thiz);


class EmulatorThread
{
public:
    explicit EmulatorThread(char* brom, char* nor);
    ~EmulatorThread();
protected:
    char* fBROMBuffer;
    char* fNorBuffer;
    bool fKeeping;
#ifndef _WIN32
    NN_PADDING3;
#endif
    void* fLCDBuffer;
#ifndef _WIN32
    nn::os::Thread fThread;
#endif
private:
    unsigned int lastTicket;
    unsigned long long totalcycle;
    //const unsigned spdc1016freq = 3686400;
    bool measured;
#ifndef _WIN32
    NN_PADDING3;
#endif
    unsigned remeasure;
    unsigned batchlimiter;
    long batchcount;
    double sleepgap;
    long sleepcount;
// protected
public:
    void run();

#ifdef AUTOTEST
private:
    bool enablelogging;
#ifndef _WIN32
    NN_PADDING3;
#endif
    void TryTest(unsigned line);
#endif

public:
    void start();

public:
    void StopKeeping();
//signals:
//    void lcdBufferChanged(QByteArray* buffer);
};

typedef TNekoDriver* PNekoDriver;
extern PNekoDriver theNekoDriver;

extern bool lcdoffshift0flag;


extern unsigned short lcdbuffaddr; // unused

extern unsigned keypadmatrix[8][8]; // char -> uint32

extern TScreenBuffer renderLCDBuffer;

#define TF_STACKOVERFLOW 0x1
#define TF_NMIFLAG 0x8
#define TF_IRQFLAG 0x10
#define TF_WATCHDOG 0x80

#endif
