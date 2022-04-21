#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class QLabel;
class QLineEdit;
class QPushButton;
class QToolButton;
class QNetworkReply;

class ElidedTextLabel;
class TaskTableWidget;


class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void changemaxtasknum();
    void startGetUserInfo();
    void startGetUFace();

private slots:
    void downloadButtonClicked();
    void getUserInfoFinished();
    void getUFaceFinished();
    void ufaceButtonClicked();
    void logoutActionTriggered();

private:
    bool hasGotUInfo = false;
    bool hasGotUFace = false;
    QNetworkReply *uinfoReply = nullptr;
    QString ufaceUrl;

    QToolButton *ufaceButton;
    ElidedTextLabel *unameLabel;
    //202220421 new add
    QLineEdit *maxtasknumedit;
    //202220421 new add
    QLineEdit *urlLineEdit;
    TaskTableWidget *taskTable;
};
#endif // MAINWINDOW_H
