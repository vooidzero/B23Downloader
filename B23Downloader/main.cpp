#include "MainWindow.h"
#include <QApplication>
#include <QLocalSocket>
#include <QLocalServer>

#ifdef Q_OS_WIN // ensure single application instance at Windows

#include <windows.h>

bool isAppAlreadyExist()
{
    CreateMutexA(nullptr, true, "B23Dld-AppMutex");
    return GetLastError() == ERROR_ALREADY_EXISTS;
}

static const auto LocalServerName = "B23Dld-LocalServer";

void createLocalServer(QWidget *mainWindow)
{
    static std::unique_ptr<QLocalServer> server;
    server = std::make_unique<QLocalServer>();
    server->setSocketOptions(QLocalServer::UserAccessOption);
    server->listen(LocalServerName);
    mainWindow->connect(server.get(), &QLocalServer::newConnection,
                        mainWindow, [server=server.get(), mainWindow](){
        /* mainWindow->showNormal() can show window (at foreground) if it's minimized.
         *
         * However, if mainWindow is at another virtual desktop, or it's not minimized and not at foreground,
         * we can't bring mainWindow to front directly. Because Microsoft doesn't allow an application to
         * interrupt what the user is currently doing in another application.
         *
         * Solution: send window handle to the new application instance, let it bring this app to foreground.
         * (Since the new app is active, it can activate another app.)
         */

        auto socket = server->nextPendingConnection();
        auto hWnd = mainWindow->winId();
        socket->write("ok\n");
        socket->write((char*)(&hWnd), sizeof(WId));
        socket->flush();
    });
}

void showExistingApp()
{
    QLocalSocket socket;
    socket.connectToServer(LocalServerName);
    if (socket.waitForConnected(3000)) {
        socket.waitForReadyRead(3000);
        auto msg = socket.readLine();
        if (msg.trimmed() != "ok") {
            return;
        }
        auto data = socket.readAll();
        if (data.size() != sizeof (WId)) {
            return;
        }
        auto hWnd = HWND(*(WId*)(data.data()));
        WINDOWPLACEMENT placement;
        GetWindowPlacement(hWnd, &placement);
        if (placement.showCmd == SW_SHOWMINIMIZED) {
            ShowWindow(hWnd, SW_RESTORE);
        } else {
            SetForegroundWindow(hWnd);
        }

    }
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    if (!isAppAlreadyExist()) {
        MainWindow w;
        createLocalServer(&w);
        w.show();
        return a.exec();
    } else {
        showExistingApp();
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
