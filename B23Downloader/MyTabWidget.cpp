#include "MyTabWidget.h"
#include <QtWidgets>

MyTabWidget::MyTabWidget(QWidget *parent) : QWidget(parent)
{
    auto layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    bar = new QToolBar;
    bar->setIconSize(QSize(28, 28));
    bar->setOrientation(Qt::Orientation::Vertical);
    stack = new QStackedWidget;
    layout->addWidget(bar);
    layout->addWidget(stack);
    actGroup = new QActionGroup(this);
    actGroup->setExclusive(true);
    connect(bar, &QToolBar::actionTriggered, this, [stack=this->stack](QAction *act) {
         stack->setCurrentIndex(act->data().toInt());
    });

    bar->setStyleSheet(
        "QToolBar{border-right:1px solid rgb(224,224,224);}"
        "QToolButton{padding:3px;border:none;margin-right:3px;}"
        "QToolButton:checked{background-color:rgb(232,232,232);}"
        "QToolButton:hover{background-color:rgb(232,232,232);}"
        "QToolButton:pressed{background-color:rgb(232,232,232);}"
    );
}

void MyTabWidget::setTabToolButtonStyle(Qt::ToolButtonStyle style)
{
    bar->setToolButtonStyle(style);
}

void MyTabWidget::addTab(QWidget *page, const QIcon &icon, const QString &label)
{
    stack->addWidget(page);

    auto idx = stack->count() - 1;
    auto act = new QAction(icon, label);
    act->setCheckable(true);
    act->setData(idx);
    if (idx == 0) {
        act->setChecked(true);
    }
    actGroup->addAction(act);
    bar->addAction(act);
}
