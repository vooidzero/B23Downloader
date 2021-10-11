VERSION = 0.9.5
QT       += core gui network

win32-g++:contains(QMAKE_HOST.arch, x86_64):{
    # DEFINES += ENABLE_UPDATE_CHECK
}

# On Windows, QApplication::applicationVersion() returns VERSION defined above
# But on Linux, QApplication::applicationVersion() returns empty string
!windows:DEFINES += APP_VERSION=\\\"$$VERSION\\\"


greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

RC_ICONS = B23Downloader.ico

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    AboutWidget.cpp \
    DownloadDialog.cpp \
    DownloadTask.cpp \
    Extractor.cpp \
    Flv.cpp \
    LoginDialog.cpp \
    MainWindow.cpp \
    MyTabWidget.cpp \
    Network.cpp \
    QrCode.cpp \
    Settings.cpp \
    TaskTable.cpp \
    main.cpp \
    utils.cpp

HEADERS += \
    AboutWidget.h \
    DownloadDialog.h \
    DownloadTask.h \
    Extractor.h \
    Flv.h \
    LoginDialog.h \
    MainWindow.h \
    MyTabWidget.h \
    Network.h \
    QrCode.h \
    Settings.h \
    TaskTable.h \
    utils.h

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    icons.qrc
