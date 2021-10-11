#ifndef ABOUTWIDGET_H
#define ABOUTWIDGET_H

#include <QWidget>

class AboutWidgetPrivate;

class AboutWidget : public QWidget
{
public:
    AboutWidget(QWidget *parent = nullptr);

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    friend class AboutWidgetPrivate;
    std::unique_ptr<AboutWidgetPrivate> d;
};

#endif // ABOUTWIDGET_H
