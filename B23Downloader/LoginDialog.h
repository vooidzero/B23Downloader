#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>
#include <QJsonValue>

class QLabel;
class QTimer;
class QToolButton;
class QNetworkReply;

class LoginDialog : public QDialog
{
    Q_OBJECT

protected:
    void closeEvent(QCloseEvent *e) override;

public:
    explicit LoginDialog(QWidget *parent = nullptr);
    ~LoginDialog() override;
    void setQrCode(const QString &content);

private:
    QJsonValue getReplyData();
    void startGetLoginUrl();

private slots:
    void getLoginUrlFinished();
    void pollLoginInfo();
    void getLoginInfoFinished();

private:
    void qrCodeExpired();
    void showRefreshButton();
    void hideRefreshButton();

    QString oauthKey;
    int polledTimes = 0;
    QTimer *pollTimer;
    QLabel *qrCodeLabel;
    QLabel *tipLabel;
    QToolButton *refreshButton;
    QNetworkReply *httpReply = nullptr;
};

#endif // LOGINDIALOG_H
