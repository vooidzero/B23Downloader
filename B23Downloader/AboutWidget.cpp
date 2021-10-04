#include "AboutWidget.h"
#include "Network.h"
#include <QtWidgets>

struct VersionInfo
{
    QString ver;
    QString exeDownloadUrl;
    QString desc;
};

static const auto ReleaseExecutableSuffix =
        "win_64.exe"
;

/**
 * @brief if versionA > versionB, return 1;
 *        if versionA < versionB, return -1;
 *        else return 0;
 */
static int cmpVersion(const QString &a, const QString &b)
{
    int lenA = a.size();
    int lenB = b.size();
    int posA = 0;
    int posB = 0;

    auto nextNum = [](const QString &ver, int &pos) -> int {
        int n = 0;
        while (pos < ver.size() && ver[pos] != '.') {
            n = n * 10 + (ver[pos].toLatin1() - '0');
            pos++;
        }
        pos++; // skip '.'
        return n;
    };

    while (!(posA >= lenA && posB >= lenB)) {
        int num1 = nextNum(a, posA);
        int num2 = nextNum(b, posB);
        if (num1 < num2) {
            return -1;
        } else if (num1 > num2) {
            return 1;
        }
    }
    return 0;
}

VersionInfo parseVerInfoFromGithub(QNetworkReply *reply)
{
    auto obj = QJsonDocument::fromJson(reply->readAll()).object();
    VersionInfo verInfo;
    verInfo.ver = obj["tag_name"].toString().sliced(1);
    verInfo.desc = obj["body"].toString();
    for (auto &&assetObjRef : obj["assets"].toArray()) {
        auto asset = assetObjRef.toObject();
        if (asset["name"].toString().endsWith(ReleaseExecutableSuffix)) {
            verInfo.exeDownloadUrl = asset["browser_download_url"].toString();
            break;
        }
    }
    return verInfo;
}

VersionInfo parseVerInfoFromNjugit(QNetworkReply *reply)
{
    auto obj = QJsonDocument::fromJson(reply->readAll()).array().first().toObject();
    VersionInfo verInfo;
    verInfo.ver = obj["tag_name"].toString().sliced(1);
    verInfo.desc = obj["description"].toString();
    for (auto &&assetObjRef : obj["assets"].toObject()["links"].toArray()) {
        auto asset = assetObjRef.toObject();
        if (asset["name"].toString().endsWith(ReleaseExecutableSuffix)) {
            verInfo.exeDownloadUrl = asset["direct_asset_url"].toString();
            break;
        }
    }
    return verInfo;
}

AboutWidget::AboutWidget(QWidget *parent) : QWidget(parent)
{
    auto layout = new QVBoxLayout(this);

    constexpr auto iconSize = QSize(64, 64);
    auto iconLabel = new QLabel;
    iconLabel->setFixedSize(iconSize);
    iconLabel->setScaledContents(true);
    iconLabel->setPixmap(QPixmap(":/icons/icon-96x96.png"));

    auto info =
        QStringLiteral("<h2>B23Downloader</h2>版本: v") + QApplication::applicationVersion() +
        QStringLiteral(
            "<br>本软件开源免费。"
            "<br>项目链接: <a href=\"https://github.com/vooidzero/B23Downloader\">GitHub</a>"
                " 或 <a href=\"https://git.nju.edu.cn/zero/B23Downloader\">NJU Git</a>"
        );
    auto infoLabel = new QLabel(info);

    auto infoLayout = new QHBoxLayout;
    infoLayout->addWidget(iconLabel);
    infoLayout->addSpacing(20);
    infoLayout->addWidget(infoLabel);
    infoLayout->setAlignment(Qt::AlignCenter);
    infoLayout->addSpacing(iconSize.width());
    layout->addSpacing(10);
    layout->addLayout(infoLayout);

    newVersionInfoTextView = new QTextEdit;
    newVersionInfoTextView->setReadOnly(true);
    newVersionInfoTextView->setFrameShape(QFrame::NoFrame);
    newVersionInfoTextView->setMinimumWidth(320);
    layout->addSpacing(10);
    layout->addWidget(newVersionInfoTextView, 1, Qt::AlignCenter);

    auto checkUpdateViaGithubBtn = new QPushButton("检查更新 (via GitHub)");
    auto checkUpdateViaNjugitBtn = new QPushButton("检查更新 (via NJU Git)");
    auto checkUpdateLayout = new QHBoxLayout;
    checkUpdateLayout->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    checkUpdateLayout->addWidget(checkUpdateViaGithubBtn);
    checkUpdateLayout->addSpacing(10);
    checkUpdateLayout->addWidget(checkUpdateViaNjugitBtn);
    layout->addLayout(checkUpdateLayout);

    connect(checkUpdateViaGithubBtn, &QPushButton::clicked, this, [this]{
        startGetVersionInfo(
            "https://api.github.com/repos/vooidzero/B23Downloader/releases/latest",
            &parseVerInfoFromGithub
        );
    });

    connect(checkUpdateViaNjugitBtn, &QPushButton::clicked, this, [this]{
        startGetVersionInfo(
            "https://git.nju.edu.cn/api/v4/projects/3171/releases",
            &parseVerInfoFromNjugit
        );
    });
}

void AboutWidget::showEvent(QShowEvent *event)
{
    newVersionInfoTextView->setText(QStringLiteral(
        "<center>こんなちいさな星座なのに</center>"
        "<center>ここにいたこと 気付いてくれて</center>"
        "<center>ありがとう！</center>"
    ));
    QWidget::showEvent(event);
}

void AboutWidget::hideEvent(QHideEvent *event)
{
    if (httpReply != nullptr) {
        httpReply->abort();
    }
    newVersionInfoTextView->clear();
    QWidget::hideEvent(event);
}

void AboutWidget::startGetVersionInfo(const QString &url, std::function<VersionInfo (QNetworkReply *)> parser)
{
    auto rqst = QNetworkRequest(QUrl(url));
    httpReply = Network::accessManager()->get(rqst);
    newVersionInfoTextView->setText("请求中...");
    setEnabled(false);
    connect(this->httpReply, &QNetworkReply::finished, this, [this, parser]{
        setEnabled(true);
        auto reply = httpReply;
        httpReply = nullptr;
        reply->deleteLater();

        if (reply->error() == QNetworkReply::OperationCanceledError) {
            return;
        } else if (reply->error() != QNetworkReply::NoError) {
            newVersionInfoTextView->setText("<font color=\"red\">网络错误</font>");
            return;
        }

        setUpdateInfoLabel(parser(reply));
    });
}

void AboutWidget::setUpdateInfoLabel(const VersionInfo &verInfo)
{
    if (verInfo.exeDownloadUrl.isEmpty()) {
        newVersionInfoTextView->clear();
        return;
    }

    if (cmpVersion(QApplication::applicationVersion(), verInfo.ver) >= 0) {
        newVersionInfoTextView->setText("当前已是最新版本, 无需更新。");
        return;
    }

     newVersionInfoTextView->setText(
        QStringLiteral("<b>版本 v") + verInfo.ver +
        QStringLiteral("</b><br>下载链接:") + verInfo.exeDownloadUrl +
        "<br>" + verInfo.desc
     );
}
