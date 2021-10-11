#include "MainWindow.h"
#include "Settings.h"
#include "Network.h"
#include "utils.h"

#include "Extractor.h"
#include "LoginDialog.h"
#include "DownloadDialog.h"
#include "TaskTable.h"
#include "AboutWidget.h"
#include "MyTabWidget.h"

#include <QtWidgets>
#include <QtNetwork>

static constexpr int GetUserInfoRetryInterval = 10000; // ms
static constexpr int GetUserInfoTimeout = 10000; // ms

MainWindow::~MainWindow() = default;


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
#ifdef APP_VERSION
    QApplication::setApplicationVersion(APP_VERSION);
#endif

    Network::accessManager()->setCookieJar(Settings::inst()->getCookieJar());
    setWindowTitle("B23Downloader");
    setCentralWidget(new QWidget);
    auto mainLayout = new QVBoxLayout(centralWidget());
    auto topLayout = new QHBoxLayout;

    // set up user info widgets
    ufaceButton = new QToolButton;
    ufaceButton->setText("登录");
    ufaceButton->setFixedSize(32, 32);
    ufaceButton->setIconSize(QSize(32, 32));
    ufaceButton->setCursor(Qt::PointingHandCursor);
    auto loginTextFont = font();
    loginTextFont.setBold(true);
    loginTextFont.setPointSize(font().pointSize() + 1);
    ufaceButton->setFont(loginTextFont);
    ufaceButton->setPopupMode(QToolButton::InstantPopup);
    ufaceButton->setStyleSheet(R"(
        QToolButton {
            color: #00a1d6;
            background-color: white;
            border: none;
        }
        QToolButton::menu-indicator { image: none; }
    )");
    connect(ufaceButton, &QToolButton::clicked, this, &MainWindow::ufaceButtonClicked);

    unameLabel = new ElidedTextLabel;
    unameLabel->setHintWidthToString("晚安玛卡巴卡！やさしい夢見てね");
    topLayout->addWidget(ufaceButton);
    topLayout->addWidget(unameLabel, 1);

    // set up download url lineEdit
    auto downloadUrlLayout = new QHBoxLayout;
    downloadUrlLayout->setSpacing(0);
    urlLineEdit = new QLineEdit;
    urlLineEdit->setFixedHeight(32);
    urlLineEdit->setClearButtonEnabled(true);
    urlLineEdit->setPlaceholderText("bilibili 直播/视频/漫画 URL");

    auto downloadButton = new QPushButton;
    downloadButton->setToolTip("下载");
    downloadButton->setFixedSize(QSize(32, 32));
    downloadButton->setIconSize(QSize(28, 28));
    downloadButton->setIcon(QIcon(":/icons/download.svg"));
    downloadButton->setCursor(Qt::PointingHandCursor);
    downloadButton->setStyleSheet(
        "QPushButton{border:1px solid gray; border-left:0px; background-color:white;}"
        "QPushButton:hover{background-color:rgb(229,229,229);}"
        "QPushButton:pressed{background-color:rgb(204,204,204);}"
    );
    connect(urlLineEdit, &QLineEdit::returnPressed, this, &MainWindow::downloadButtonClicked);
    connect(downloadButton, &QPushButton::clicked, this, &MainWindow::downloadButtonClicked);

    downloadUrlLayout->addWidget(urlLineEdit, 1);
    downloadUrlLayout->addWidget(downloadButton);
    topLayout->addLayout(downloadUrlLayout, 2);
    mainLayout->addLayout(topLayout);

    taskTable = new TaskTableWidget;
    QTimer::singleShot(0, this, [this]{ taskTable->load(); });
    auto tabs = new MyTabWidget;
    tabs->addTab(taskTable, QIcon(":/icons/download.svg"), "正在下载");
    tabs->addTab(new AboutWidget, QIcon(":/icons/about.svg"), "关于");
    mainLayout->addWidget(tabs);

    setStyleSheet("QMainWindow{background-color:white;}QTableWidget{border:none;}");
    setMinimumSize(650, 360);
    QTimer::singleShot(0, this, [this]{ resize(minimumSize()); });

    urlLineEdit->setFocus();
    startGetUserInfo();
    //    auto reply = B23Api::get("https://www.bilibili.com/blackboard/topic/activity-4AL5_Jqb3.html");
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    auto dlg = QMessageBox(QMessageBox::Warning, "退出", "是否退出？", QMessageBox::NoButton, this);
    dlg.addButton("确定", QMessageBox::AcceptRole);
    dlg.addButton("取消", QMessageBox::RejectRole);
    auto ret = dlg.exec();
    if (ret == QMessageBox::AcceptRole) {
        taskTable->stopAll();
        taskTable->save();
        event->accept();
    } else {
        event->ignore();
    }
}

void MainWindow::startGetUserInfo()
{
    if (!Settings::inst()->hasCookies()) {
        return;
    }
    if (hasGotUInfo || uinfoReply != nullptr) {
        return;
    }
    unameLabel->setText("登录中...", Qt::gray);
    auto rqst = Network::Bili::Request(QUrl("https://api.bilibili.com/nav"));
    rqst.setTransferTimeout(GetUserInfoTimeout);
    uinfoReply = Network::accessManager()->get(rqst);;
    connect(uinfoReply, &QNetworkReply::finished, this, &MainWindow::getUserInfoFinished);
}

void MainWindow::getUserInfoFinished()
{
    auto reply = uinfoReply;
    uinfoReply->deleteLater();
    uinfoReply = nullptr;

    if (reply->error() == QNetworkReply::OperationCanceledError) {
        unameLabel->setErrText("网络请求超时");
        QTimer::singleShot(GetUserInfoRetryInterval, this, &MainWindow::startGetUserInfo);
        return;
    }

    const auto [json, errorString] = Network::Bili::parseReply(reply, "data");

    if (!json.empty() && !errorString.isNull()) {
        // cookies is wrong, or expired?
        unameLabel->clear();
        Settings::inst()->removeCookies();
    } else if (!errorString.isNull()) {
        unameLabel->setErrText(errorString);
        QTimer::singleShot(GetUserInfoRetryInterval, this, &MainWindow::startGetUserInfo);
    } else {
        // success
        hasGotUInfo = true;
        auto data = json["data"];
        auto uname = data["uname"].toString();
        ufaceUrl = data["face"].toString() + "@64w_64h.png";
        if (data["vipStatus"].toInt()) {
            unameLabel->setText(uname, B23Style::Pink);
        } else {
            unameLabel->setText(uname);
        }

        auto logoutAction = new QAction(QIcon(":/icons/logout.svg"), "退出");
        ufaceButton->addAction(logoutAction);
        ufaceButton->setIcon(QIcon(":/icons/akkarin.png"));
        connect(logoutAction, &QAction::triggered, this, &MainWindow::logoutActionTriggered);

        startGetUFace();
    }
}

void MainWindow::logoutActionTriggered()
{
    hasGotUInfo = false;
    hasGotUFace = false;
    ufaceUrl.clear();

    unameLabel->clear();
    ufaceButton->setIcon(QIcon());
    auto actions = ufaceButton->actions();
    if (!actions.isEmpty()) {
        ufaceButton->removeAction(actions.first());
    }

    if (uinfoReply != nullptr) {
        uinfoReply->abort();
    }

    auto settings = Settings::inst();
    auto exitPostData = "biliCSRF=" + settings->getCookieJar()->getCookie("bili_jct");
    auto exitReply = Network::Bili::postUrlEncoded("https://passport.bilibili.com/login/exit/v2", exitPostData);
    connect(exitReply, &QNetworkReply::finished, this, [=]{ exitReply->deleteLater(); });
    settings->removeCookies();
}

void MainWindow::startGetUFace()
{
    if (ufaceUrl.isNull()) {
        return;
    }
    if (hasGotUFace || uinfoReply != nullptr) {
        return;
    }

    auto rqst = Network::Bili::Request(ufaceUrl);
    rqst.setTransferTimeout(GetUserInfoTimeout);
    uinfoReply = Network::accessManager()->get(rqst);
    connect(uinfoReply, &QNetworkReply::finished, this, &MainWindow::getUFaceFinished);
}

void MainWindow::getUFaceFinished()
{
    auto reply = uinfoReply;
    uinfoReply->deleteLater();
    uinfoReply = nullptr;

    if (!hasGotUInfo && reply->error() == QNetworkReply::OperationCanceledError) {
        // aborted
        return;
    }
    if (reply->error() != QNetworkReply::NoError) {
        QTimer::singleShot(GetUserInfoRetryInterval, this, &MainWindow::startGetUFace);
        return;
    }

    hasGotUFace = true;
    QPixmap pixmap;
    pixmap.loadFromData(reply->readAll());
    ufaceButton->setIcon(QIcon(pixmap));
}

// login
void MainWindow::ufaceButtonClicked()
{
    auto settings = Settings::inst();
    if (hasGotUInfo) {
        return;
    } else if (settings->hasCookies()) {
        // is trying to get user info
        if (uinfoReply != nullptr) {
            // is requesting
            return;
        }
        // retry immediately
        startGetUserInfo();
    } else {
        settings->getCookieJar()->clear(); // remove unnecessary cookie
        auto dlg = new LoginDialog(this);
        connect(dlg, &QDialog::finished, this, [=](int result) {
            dlg->deleteLater();
            if (result == QDialog::Accepted) {
                settings->saveCookies();
                startGetUserInfo();
            } else {
                settings->getCookieJar()->clear(); // remove unnecessary cookie
            }
        });
        dlg->open();
    }
}

void MainWindow::downloadButtonClicked()
{
    auto trimmed = urlLineEdit->text().trimmed();
    if (trimmed.isEmpty()) {
        urlLineEdit->clear();
        return;
    }

    auto dlg = new DownloadDialog(trimmed, this);
    connect(dlg, &QDialog::finished, this, [this, dlg](int result) {
        dlg->deleteLater();
        // urlLineEdit->clear();
        if (result == QDialog::Accepted) {
            taskTable->addTasks(dlg->getDownloadTasks());
        }
    });
    dlg->open();
}
