#ifndef ABOUTWIDGET_H
#define ABOUTWIDGET_H

#include <QWidget>

class QTextEdit;
class QNetworkReply;

struct VersionInfo;

class AboutWidget : public QWidget
{
public:
    AboutWidget(QWidget *parent = nullptr);

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    QTextEdit *newVersionInfoTextView;
    QNetworkReply *httpReply = nullptr;

    void startGetVersionInfo(const QString &url, std::function<VersionInfo(QNetworkReply*)> parser);
    void setUpdateInfoLabel(const VersionInfo &);
};

#endif // ABOUTWIDGET_H
