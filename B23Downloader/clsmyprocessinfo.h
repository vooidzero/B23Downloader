#ifndef CLSMYPROCESSINFO_H
#define CLSMYPROCESSINFO_H

#include <QMainWindow>
#include <QObject>
#include <QQuickItem>
#include <QSharedDataPointer>
#include <QWidget>

#include "string.h"
#include "stdio.h"
class clsmyprocessinfo
{
//    Q_OBJECT
public:
    clsmyprocessinfo();
    ~clsmyprocessinfo();
    int getcurprocessid();
    char* getcurprocessidstr();
    char* strtochar(std::string);
    char* mystrcat(char*,char*);

};
//int clsmyprocessinfo::getcurprocessid();
#endif // CLSMYPROCESSINFO_H
