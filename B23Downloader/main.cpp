#include "MainWindow.h"
#include <QApplication>
#include <QSharedMemory>
#include "clsmyprocessinfo.h"
#include "string.h"
#include "stdio.h"
#include "globalv.h"
#ifdef Q_OS_WIN // ensure single application instance at Windows

#include <windows.h>

void raiseWindow(const HWND hWnd)
{
    WINDOWPLACEMENT placement;
    GetWindowPlacement(hWnd, &placement);
    if (placement.showCmd == SW_SHOWMINIMIZED) {
        ShowWindow(hWnd, SW_RESTORE);
    } else {
        SetForegroundWindow(hWnd);
    }
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    clsmyprocessinfo *mypi=new clsmyprocessinfo();
    std::string str1="B23Dl-";
    char* char1=mypi->strtochar(str1);
    char* char2=mypi->getcurprocessidstr();
    mypid=char2;
    strcat(char1,char2);
    QSharedMemory sharedMem(char1);
//    QSharedMemory sharedMem(charmemid);
    auto setHwnd = [&sharedMem](HWND hWnd) {
        sharedMem.lock();
        auto ptr = static_cast<HWND*>(sharedMem.data());
        *ptr = hWnd;
        sharedMem.unlock();
    };

    auto getHwnd = [&sharedMem]() -> HWND {
        sharedMem.attach(QSharedMemory::ReadOnly);
        sharedMem.lock();
        HWND hWnd = *static_cast<const HWND*>(sharedMem.constData());
        sharedMem.unlock();
        return hWnd;
    };

    bool isNoAppAlreadyExist = sharedMem.create(sizeof(HWND));
    if (isNoAppAlreadyExist) {
        MainWindow w;
        setHwnd((HWND)w.winId());
        w.show();
        return a.exec();
    } else {
        raiseWindow(getHwnd());
        return 0;
    }
}

#else // non-Windows platform
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    return a.exec();
}
#endif
