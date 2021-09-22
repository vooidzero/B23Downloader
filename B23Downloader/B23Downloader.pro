QT       += core gui network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

# RC_ICONS = B23Downloader.ico

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    DownloadDialog.cpp \
    DownloadTask.cpp \
    Extractor.cpp \
    LoginDialog.cpp \
    MainWindow.cpp \
    Network.cpp \
    QrCode.cpp \
    Settings.cpp \
    TaskTable.cpp \
    main.cpp \
    utils.cpp

HEADERS += \
    DownloadDialog.h \
    DownloadTask.h \
    Extractor.h \
    LoginDialog.h \
    MainWindow.h \
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
