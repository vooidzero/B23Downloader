#include "AboutWidget.h"
#include "Network.h"
#include <QtWidgets>

struct VersionInfo;

class AboutWidgetPrivate
{
    AboutWidget *p;
public:
    AboutWidgetPrivate(AboutWidget *p): p(p) {}
    QTextEdit *textView;

#ifdef ENABLE_UPDATE_CHECK
    std::unique_ptr<QNetworkReply> httpReply;
    void startGetVersionInfo(const QString &url, std::function<VersionInfo(QNetworkReply*)> parser);
    void setUpdateInfoLabel(const VersionInfo &);
#endif // ENABLE_UPDATE_CHECK
};


#ifdef ENABLE_UPDATE_CHECK
struct VersionInfo
{
    QString ver;
    QString exeDownloadUrl;
    QString desc;
};

static const auto ReleaseExecutableSuffix = "exe";

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

void AboutWidgetPrivate::startGetVersionInfo(const QString &url, std::function<VersionInfo (QNetworkReply *)> parser)
{
    auto rqst = QNetworkRequest(QUrl(url));
    httpReply.reset(Network::accessManager()->get(rqst));
    textView->setText("请求中...");
    p->setEnabled(false);
    p->connect(httpReply.get(), &QNetworkReply::finished, p, [this, parser]{
        p->setEnabled(true);
        auto reply = httpReply.release();
        reply->deleteLater();

        if (reply->error() == QNetworkReply::OperationCanceledError) {
            return;
        } else if (reply->error() != QNetworkReply::NoError) {
            textView->setText("<font color=\"red\">网络错误</font>");
            return;
        }

        setUpdateInfoLabel(parser(reply));
    });
}

void AboutWidgetPrivate::setUpdateInfoLabel(const VersionInfo &verInfo)
{
    if (verInfo.exeDownloadUrl.isEmpty()) {
        textView->clear();
        return;
    }

    if (cmpVersion(QApplication::applicationVersion(), verInfo.ver) >= 0) {
        textView->setText("当前已是最新版本, 无需更新。");
        return;
    }

     textView->setText(
        QStringLiteral("<b>版本 v") + verInfo.ver +
        QStringLiteral("</b><br>下载链接: <a href=\"") + verInfo.exeDownloadUrl + "\">" + verInfo.exeDownloadUrl +
        "</a><br>" + verInfo.desc
     );
}
#endif // ENABLE_UPDATE_CHECK



AboutWidget::AboutWidget(QWidget *parent)
    : QWidget(parent), d(new AboutWidgetPrivate(this))
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
    infoLabel->setOpenExternalLinks(true);

    auto infoLayout = new QHBoxLayout;
    infoLayout->addWidget(iconLabel);
    infoLayout->addSpacing(20);
    infoLayout->addWidget(infoLabel);
    infoLayout->setAlignment(Qt::AlignCenter);
    infoLayout->addSpacing(iconSize.width());
    layout->addSpacing(10);
    layout->addLayout(infoLayout);

    auto textView = new QTextBrowser;
    d->textView = textView;
    textView->setOpenExternalLinks(true);
    textView->setFrameShape(QFrame::NoFrame);
    textView->setMinimumWidth(320);
    layout->addSpacing(10);
    layout->addWidget(textView, 1, Qt::AlignCenter);

#ifdef ENABLE_UPDATE_CHECK
    auto checkUpdateViaGithubBtn = new QPushButton("检查更新 (via GitHub)");
    auto checkUpdateViaNjugitBtn = new QPushButton("检查更新 (via NJU Git)");
    auto checkUpdateLayout = new QHBoxLayout;
    checkUpdateLayout->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    checkUpdateLayout->addWidget(checkUpdateViaGithubBtn);
    checkUpdateLayout->addSpacing(10);
    checkUpdateLayout->addWidget(checkUpdateViaNjugitBtn);
    layout->addLayout(checkUpdateLayout);

    connect(checkUpdateViaGithubBtn, &QPushButton::clicked, this, [this]{
        d->startGetVersionInfo(
            "https://api.github.com/repos/vooidzero/B23Downloader/releases/latest",
            &parseVerInfoFromGithub
        );
    });

    connect(checkUpdateViaNjugitBtn, &QPushButton::clicked, this, [this]{
        d->startGetVersionInfo(
            "https://git.nju.edu.cn/api/v4/projects/3171/releases",
            &parseVerInfoFromNjugit
        );
    });
#endif // ENABLE_UPDATE_CHECK
}

void AboutWidget::showEvent(QShowEvent *event)
{
    d->textView->setText(QStringLiteral(
        "<center>こんなちいさな星座なのに</center>"
        "<center>ここにいたこと 気付いてくれて</center>"
        "<center>ありがとう！</center>"
    ));
    QWidget::showEvent(event);
}

void AboutWidget::hideEvent(QHideEvent *event)
{
#ifdef ENABLE_UPDATE_CHECK
    if (d->httpReply != nullptr) {
        d->httpReply->abort();
    }
#endif
    d->textView->clear();
    QWidget::hideEvent(event);
}
