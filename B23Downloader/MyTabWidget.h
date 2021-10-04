#ifndef MYTABWIDGET_H
#define MYTABWIDGET_H

#include <QWidget>

class QToolBar;
class QActionGroup;
class QStackedWidget;

class MyTabWidget : public QWidget
{
    QToolBar *bar;
    QActionGroup *actGroup;
    QStackedWidget *stack;

public:
    MyTabWidget(QWidget *parent = nullptr);
    void setTabToolButtonStyle(Qt::ToolButtonStyle);
    void addTab(QWidget *page, const QIcon &icon, const QString &label);
};

#endif // MYTABWIDGET_H
