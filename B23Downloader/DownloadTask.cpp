// Created by voidzero <vooidzero.github@qq.com>

#include "DownloadTask.h"
#include "Extractor.h"
#include "Network.h"
#include "utils.h"
#include "Flv.h"
#include <QtNetwork>

// 127: 8K 超高清
// 126: 杜比视界
// 125: HDR 真彩
static QMap<int, QString> videoQnDescMap {
    {120, "4K 超清"},
    {116, "1080P 60帧"},
    {112, "1080P 高码率"},
    {80, "1080P 高清"},
    {74, "720P 60帧"},
    {64, "720P 高清"},
    {32, "480P 清晰"},
    {16, "360P 流畅"},
};

static QMap<int, QString> liveQnDescMap {
    {10000, "原画"},
    {400, "蓝光"},
    {250, "超清"},
    {150, "高清"},
    {80, "流畅"}
};


inline bool jsonValue2Bool(const QJsonValue &val, bool defaultVal = false) {
    if (val.isNull() || val.isUndefined()) {
        return defaultVal;
    }
    if (val.isBool()) {
        return val.toBool();
    }
    if (val.isDouble()) {
        return static_cast<bool>(val.toInt());
    }
    if (val.isString()) {
        auto str = val.toString().toLower();
        if (str == "1" || str == "true") {
            return true;
        }
        if (str == "0" || str == "false") {
            return false;
        }
    }
    return defaultVal;
}


AbstractDownloadTask::~AbstractDownloadTask()
{
    if (httpReply != nullptr) {
        httpReply->abort();
    }
}

QString AbstractDownloadTask::getTitle() const
{
    return QFileInfo(path).baseName();
}

AbstractDownloadTask *AbstractDownloadTask::fromJsonObj(const QJsonObject &json)
{
    int type = json["type"].toInt(-1);
    switch (type) {
    case static_cast<int>(ContentType::PGC):
        return new PgcDownloadTask(json);
    case static_cast<int>(ContentType::PUGV):
        return new PugvDownloadTask(json);
    case static_cast<int>(ContentType::UGC):
        return new UgcDownloadTask(json);
    case static_cast<int>(ContentType::Comic):
        return new ComicDownloadTask(json);
    }
    return nullptr;
}

QJsonValue AbstractDownloadTask::getReplyJson(const QString &dataKey)
{
    auto reply = this->httpReply;
    this->httpReply = nullptr;
    reply->deleteLater();

    // abort() is called.
    if (reply->error() == QNetworkReply::OperationCanceledError) {
        return QJsonValue();
    }

    const auto [json, errorString] = Network::Bili::parseReply(reply, dataKey);

    if (!errorString.isNull()) {
        emit errorOccurred(errorString);
        return QJsonValue();
    }

    if (dataKey.isEmpty()) {
        return json;
    } else {
        return json[dataKey];
    }
}



AbstractVideoDownloadTask::~AbstractVideoDownloadTask() = default;

qint64 AbstractVideoDownloadTask::getDownloadedBytesCnt() const
{
    return downloadedBytesCnt;
}

void AbstractVideoDownloadTask::startDownload()
{
    httpReply = getPlayUrlInfo();
    connect(httpReply, &QNetworkReply::finished, this, [this]{
        auto data = getReplyJson(getPlayUrlInfoDataKey()).toObject();
        if (data.isEmpty()) {
            return;
        }
        parsePlayUrlInfo(data);
    });
}

void AbstractVideoDownloadTask::stopDownload()
{
    if (httpReply != nullptr) {
        httpReply->abort();
    }
}



QJsonObject VideoDownloadTask::toJsonObj() const
{
    return QJsonObject{
        {"path", path},
        {"qn", qn},
        {"bytes", downloadedBytesCnt},
        {"total", totalBytesCnt}
    };
}

VideoDownloadTask::VideoDownloadTask(const QJsonObject &json)
    : AbstractVideoDownloadTask(json["path"].toString(), json["qn"].toInt())
{
    downloadedBytesCnt = json["bytes"].toInteger(0);
    totalBytesCnt = json["total"].toInteger(0);
}

void VideoDownloadTask::removeFile()
{
    QFile::remove(path);
}

int VideoDownloadTask::estimateRemainingSeconds(qint64 downBytesPerSec) const
{
    if (downBytesPerSec == 0 || totalBytesCnt == 0) {
        return -1;
    }
    qint64 ret = (totalBytesCnt - downloadedBytesCnt) / downBytesPerSec;
    return (ret > INT32_MAX ? -1 : static_cast<int>(ret));
}

double VideoDownloadTask::getProgress() const
{
    if (totalBytesCnt == 0) {
        return 0;
    }
    return static_cast<double>(downloadedBytesCnt) / totalBytesCnt;
}

QString VideoDownloadTask::getProgressStr() const
{
    if (totalBytesCnt == 0) {
        return QString();
    }
    return QStringLiteral("%1/%2").arg(
        Utils::formattedDataSize(downloadedBytesCnt),
        Utils::formattedDataSize(totalBytesCnt)
    );
}

QnList VideoDownloadTask::getAllPossibleQn()
{
    return videoQnDescMap.keys();
}

QString VideoDownloadTask::getQnDescription(int qn)
{
    return videoQnDescMap.value(qn);
}

QString VideoDownloadTask::getQnDescription() const
{
    return getQnDescription(qn);
}

QnInfo VideoDownloadTask::getQnInfoFromPlayUrlInfo(const QJsonObject &data)
{
    QnInfo qnInfo;
    for (auto &&fmtValR : data["support_formats"].toArray()) {
        auto fmtObj = fmtValR.toObject();
        auto qn = fmtObj["quality"].toInt();
        auto desc = fmtObj["new_description"].toString();
        qnInfo.qnList.append(qn);
        if (videoQnDescMap.value(qn) != desc) {
            videoQnDescMap.insert(qn, desc);
        }
    }
    qnInfo.currentQn = data["quality"].toInt();
    return qnInfo;
}

bool VideoDownloadTask::checkQn(int qnFromReply)
{
    if (qnFromReply != qn) {
        if (downloadedBytesCnt == 0) {
            qn = qnFromReply;
        } else {
            emit errorOccurred("获取到画质与已下载部分不同. 请确定登录/会员状态");
            return false;
        }
    }

    return true;
}

bool VideoDownloadTask::checkSize(qint64 sizeFromReply)
{
    if (totalBytesCnt != sizeFromReply) {
        if (downloadedBytesCnt > 0) {
            emit errorOccurred("获取到文件大小与先前不一致");
            return false;
        } else {
            totalBytesCnt = sizeFromReply;
        }
    }
    return true;
}

void VideoDownloadTask::parsePlayUrlInfo(const QJsonObject &data)
{
    if (jsonValue2Bool(data["is_preview"], 0)) {
        if (!jsonValue2Bool(data["has_paid"], 1)) {
            emit errorOccurred("该视频需要大会员/付费");
            return;
        } /* else {
            emit errorOccurred("该视频为预览");
            return;
        } */
    }

    auto qnInfo = getQnInfoFromPlayUrlInfo(data);
    if (!checkQn(qnInfo.currentQn)) {
        return;
    }

    auto durl = data["durl"].toArray();
    if (durl.size() == 0) {
        emit errorOccurred("请求错误: durl 为空");
        return;
    } else if (durl.size() > 1) {
        emit errorOccurred("该视频当前画质有分段(不支持)");
        return;
    }

    auto durlObj = durl.first().toObject();
    if (!checkSize(durlObj["size"].toInteger())) {
        return;
    }

    durationInMSec = durlObj["length"].toInt();
    startDownloadStream(durlObj["url"].toString());
}

std::unique_ptr<QFile> VideoDownloadTask::openFileForWrite()
{
    auto dir = QFileInfo(path).absolutePath();
    if (!QFileInfo::exists(dir)) {
        if (!QDir().mkpath(dir)) {
            emit errorOccurred("创建目录失败");
            return nullptr;
        }
    }

    auto file = std::make_unique<QFile>(path);
    // WriteOnly: QFile implies Truncate (All earlier contents are lost)
    //              unless combined with ReadOnly, Append or NewOnly.
    if (!file->open(QIODevice::ReadWrite)) {
        emit errorOccurred("打开文件失败");
        return nullptr;
    }

    auto fileSize = file->size();
    if (fileSize < downloadedBytesCnt) {
        qDebug() << QString("filesize(%1) < bytes(%2)").arg(fileSize).arg(downloadedBytesCnt);
        downloadedBytesCnt = fileSize;
    }
    file->seek(downloadedBytesCnt);
    return file;
}

void VideoDownloadTask::startDownloadStream(const QUrl &url)
{
    emit getUrlInfoFinished();

    // check extension of filename
    auto ext = Utils::fileExtension(url.fileName());
    if (downloadedBytesCnt == 0 && !path.endsWith(ext, Qt::CaseInsensitive)) {
        path.append(ext);
    }

    file = openFileForWrite();
    if (!file) {
        return;
    }

    auto request = Network::Bili::Request(url);
    if (downloadedBytesCnt != 0) {
        request.setRawHeader("Range", "bytes=" + QByteArray::number(downloadedBytesCnt) + "-");
    }

    httpReply = Network::accessManager()->get(request);
    connect(httpReply, &QNetworkReply::readyRead, this, &VideoDownloadTask::onStreamReadyRead);
    connect(httpReply, &QNetworkReply::finished, this, &VideoDownloadTask::onStreamFinished);
}

void VideoDownloadTask::onStreamFinished()
{
    auto reply = httpReply;
    httpReply->deleteLater();
    httpReply = nullptr;

    file.reset();

    if (reply->error() == QNetworkReply::OperationCanceledError) {
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        emit errorOccurred("网络请求错误");
        return;
    }

    emit downloadFinished();
}

void VideoDownloadTask::onStreamReadyRead()
{
    auto tmp = downloadedBytesCnt + httpReply->bytesAvailable();
    Q_ASSERT(file != nullptr);
    if (-1 == file->write(httpReply->readAll())) {
        emit errorOccurred("文件写入失败: " + file->errorString());
        httpReply->abort();
    } else {
        downloadedBytesCnt = tmp;
    }
}



QJsonObject PgcDownloadTask::toJsonObj() const
{
    auto json = VideoDownloadTask::toJsonObj();
    json.insert("type", static_cast<int>(ContentType::PGC));
    json.insert("ssid", ssId);
    json.insert("epid", epId);
    return json;
}

PgcDownloadTask::PgcDownloadTask(const QJsonObject &json)
    : VideoDownloadTask(json),
      ssId(json["ssid"].toInteger()),
      epId(json["epid"].toInteger())
{
}

QNetworkReply *PgcDownloadTask::getPlayUrlInfo(qint64 epId, int qn)
{
    auto api = "https://api.bilibili.com/pgc/player/web/playurl";
    auto query = QString("?ep_id=%1&qn=%2&fourk=1").arg(epId).arg(qn);
    return Network::Bili::get(api + query);
}

QNetworkReply *PgcDownloadTask::getPlayUrlInfo() const
{
    return getPlayUrlInfo(epId, qn);
}

const QString PgcDownloadTask::playUrlInfoDataKey = "result";

QString PgcDownloadTask::getPlayUrlInfoDataKey() const
{
    return playUrlInfoDataKey;
}



QJsonObject PugvDownloadTask::toJsonObj() const
{
    auto json = VideoDownloadTask::toJsonObj();
    json.insert("type", static_cast<int>(ContentType::PUGV));
    json.insert("ssid", ssId);
    json.insert("epid", epId);
    return json;
}

PugvDownloadTask::PugvDownloadTask(const QJsonObject &json)
    : VideoDownloadTask(json),
      ssId(json["ssid"].toInteger()),
      epId(json["epid"].toInteger())
{
}

QNetworkReply *PugvDownloadTask::getPlayUrlInfo(qint64 epId, int qn)
{
    auto api = "https://api.bilibili.com/pugv/player/web/playurl";
    auto query = QString("?ep_id=%1&qn=%2&fourk=1").arg(epId).arg(qn);
    return Network::Bili::get(api + query);
}

QNetworkReply *PugvDownloadTask::getPlayUrlInfo() const
{
    return getPlayUrlInfo(epId, qn);
}

const QString PugvDownloadTask::playUrlInfoDataKey = "data";

QString PugvDownloadTask::getPlayUrlInfoDataKey() const
{
    return playUrlInfoDataKey;
}



QJsonObject UgcDownloadTask::toJsonObj() const
{
    auto json = VideoDownloadTask::toJsonObj();
    json.insert("type", static_cast<int>(ContentType::UGC));
    json.insert("aid", aid);
    json.insert("cid", cid);
    return json;
}

UgcDownloadTask::UgcDownloadTask(const QJsonObject &json)
    : VideoDownloadTask(json),
      aid(json["aid"].toInteger()),
      cid(json["cid"].toInteger())
{
}

QNetworkReply *UgcDownloadTask::getPlayUrlInfo(qint64 aid, qint64 cid, int qn)
{
    auto api = "https://api.bilibili.com/x/player/playurl";
    auto query = QString("?avid=%1&cid=%2&qn=%3&fourk=1").arg(aid).arg(cid).arg(qn);
    return Network::Bili::get(api + query);
}

QNetworkReply *UgcDownloadTask::getPlayUrlInfo() const
{
    return getPlayUrlInfo(aid, cid, qn);
}

const QString UgcDownloadTask::playUrlInfoDataKey = "data";

QString UgcDownloadTask::getPlayUrlInfoDataKey() const
{
    return playUrlInfoDataKey;
}



QNetworkReply *LiveDownloadTask::getPlayUrlInfo(qint64 roomId, int qn)
{
    auto api = "https://api.live.bilibili.com/xlive/web-room/v2/index/getRoomPlayInfo";
    auto query = QString("?protocol=0,1&format=0,1,2&codec=0,1&room_id=%1&qn=%2").arg(roomId).arg(qn);
    return Network::Bili::get(api + query);
}

QNetworkReply *LiveDownloadTask::getPlayUrlInfo() const
{
    return getPlayUrlInfo(roomId, qn);
}

const QString LiveDownloadTask::playUrlInfoDataKey = "data";

QString LiveDownloadTask::getPlayUrlInfoDataKey() const
{
    return playUrlInfoDataKey;
}

LiveDownloadTask::LiveDownloadTask(qint64 roomId, int qn, const QString &path)
    : AbstractVideoDownloadTask(QString(), qn), basePath(path), roomId(roomId)
{
}

LiveDownloadTask::~LiveDownloadTask() = default;

QJsonObject LiveDownloadTask::toJsonObj() const
{
    return QJsonObject();
}

//LiveDownloadTask::LiveDownloadTask(const QJsonObject &json)
//{

//}

QString LiveDownloadTask::getTitle() const
{
    return QFileInfo(basePath).baseName();
}

void LiveDownloadTask::removeFile()
{
    // don't delete
}

int LiveDownloadTask::estimateRemainingSeconds(qint64 downBytesPerSec) const
{
    Q_UNUSED(downBytesPerSec)
    // return duration of downloaded video instead
    return (dldDelegate == nullptr ? 0 : dldDelegate->getDurationInMSec() / 1000);
}

QString LiveDownloadTask::getProgressStr() const
{
    if (downloadedBytesCnt == 0) {
        return QString();
    }
    return Utils::formattedDataSize(downloadedBytesCnt);
}

QnList LiveDownloadTask::getAllPossibleQn()
{
    return liveQnDescMap.keys();
}

QString LiveDownloadTask::getQnDescription(int qn)
{
    return liveQnDescMap.value(qn);
}

QString LiveDownloadTask::getQnDescription() const
{
    return getQnDescription(qn);
}

QnInfo LiveDownloadTask::getQnInfoFromPlayUrlInfo(const QJsonObject &data)
{
    QnInfo qnInfo;
    auto infoObj = data["playurl_info"].toObject()["playurl"].toObject();

    QMap<int, QString> m;
    for (auto &&qnDescValR : infoObj["g_qn_desc"].toArray()) {
        auto qnDescObj = qnDescValR.toObject();
        auto qn = qnDescObj["qn"].toInt();
        auto desc = qnDescObj["desc"].toString();
        m[qn] = desc;
    }
    auto obj = infoObj["stream"].toArray().first()
                ["format"].toArray().first()
                ["codec"].toArray().first();
    qnInfo.currentQn = obj["current_qn"].toInt();
    for (auto &&qnValR : obj["accept_qn"].toArray()) {
        auto qn = qnValR.toInt();
        qnInfo.qnList.append(qn);
        if (liveQnDescMap.value(qn) != m[qn]) {
            liveQnDescMap.insert(qn, m[qn]);
        }
    }
    return qnInfo;
}

QString LiveDownloadTask::getPlayUrlFromPlayUrlInfo(const QJsonObject &data)
{
    auto urlObj = data["playurl_info"].toObject()
                ["playurl"].toObject()
                ["stream"].toArray().first()
                ["format"].toArray().first()
                ["codec"].toArray().first();
    auto baseUrl = urlObj["base_url"].toString();
    auto obj = urlObj["url_info"].toArray().first();
    auto host = obj["host"].toString();
    auto extra = obj["extra"].toString();
    return host + baseUrl + extra;
}

void LiveDownloadTask::parsePlayUrlInfo(const QJsonObject &data)
{
    if (data["live_status"].toInt() != 1) {
        emit errorOccurred("未开播或正在轮播");
        return;
    }
    qn = getQnInfoFromPlayUrlInfo(data).currentQn;
    auto url = getPlayUrlFromPlayUrlInfo(data);
    auto ext = Utils::fileExtension(QUrl(url).fileName());
    if (ext != ".flv") {
        emit errorOccurred("非FLV");
        return;
    }

    emit getUrlInfoFinished();

    downloadedBytesCnt = 0;    
    httpReply = Network::Bili::get(url);
    dldDelegate = std::make_unique<FlvLiveDownloadDelegate>(*httpReply, [this](){
        auto dateStr = QDateTime::currentDateTime().toString("[yyyy.MM.dd] hh.mm.ss");
        auto path = basePath + " " + dateStr + ".flv";
        auto file = std::make_unique<QFile>(path);
        if (file->open(QIODevice::WriteOnly)) {
            this->path = std::move(path);
            return file;
        } else {
            return decltype(file)();
        }
    });

    connect(httpReply, &QNetworkReply::readyRead, this, [this]() {
        auto ret = dldDelegate->newDataArrived();
        if (!ret) {
            httpReply->abort();
            emit errorOccurred(dldDelegate->errorString());
        }
        downloadedBytesCnt = dldDelegate->getReadBytesCnt() + httpReply->bytesAvailable();
    });

    connect(httpReply, &QNetworkReply::finished, this, [this](){
        auto reply = httpReply;
        httpReply = nullptr;
        reply->deleteLater();
        dldDelegate.reset();

        if (reply->error() == QNetworkReply::OperationCanceledError) {
            return;
        } else if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred("网络请求错误");
        } else {
            emit errorOccurred("已结束或下载速度过慢");
        }
    });
}


ComicDownloadTask::~ComicDownloadTask() = default;

QJsonObject ComicDownloadTask::toJsonObj() const
{
    return QJsonObject {
        {"type", static_cast<int>(ContentType::Comic)},
        {"path", path},
        {"id", comicId},
        {"epid", epId},
        {"imgs", finishedImgCnt},
        {"bytes", bytesCntTillLastImg},
        {"total", totalImgCnt},
    };
}

ComicDownloadTask::ComicDownloadTask(const QJsonObject &json)
    : AbstractDownloadTask(json["path"].toString()),
      comicId(json["id"].toInteger()),
      epId(json["epid"].toInteger()),
      totalImgCnt(json["total"].toInt(0)),
      finishedImgCnt(json["imgs"].toInt(0)),
      bytesCntTillLastImg(json["bytes"].toInteger(0))
{
}

void ComicDownloadTask::startDownload()
{
    auto getImgPathsUrl = "https://manga.bilibili.com/twirp/comic.v1.Comic/GetImageIndex?device=pc&platform=web";
//    auto postData = "{\"ep_id\":" + QByteArray::number(epid) + "}";
//    httpReply = Network::postJson(getImgPathsUrl, postData);
    httpReply = Network::Bili::postJson(getImgPathsUrl, {{"ep_id", epId}});
    connect(httpReply, &QNetworkReply::finished, this, &ComicDownloadTask::getImgInfoFinished);
}

void ComicDownloadTask::stopDownload()
{
    if (httpReply != nullptr) {
        httpReply->abort();
    }
}

void ComicDownloadTask::removeFile()
{
    // simple but may delete innocent files !!!
    QDir(path).removeRecursively();
}

void ComicDownloadTask::getImgInfoFinished()
{
    auto data = getReplyJson("data").toObject();
    if (data.isEmpty()) {
        return;
    }

    // assert: imgRqstPaths.isEmpty()

    auto images = data["images"].toArray();
    totalImgCnt = images.size();
    imgRqstPaths.reserve(totalImgCnt);
    for (auto &&imgObjRef : images) {
        auto imgObj = imgObjRef.toObject();
        imgRqstPaths.append(imgObj["path"].toString());
    }
    emit getUrlInfoFinished();
    downloadNextImg();
}

void ComicDownloadTask::downloadNextImg()
{
    if (finishedImgCnt == totalImgCnt) {
        emit downloadFinished();
        return;
    }
    auto getTokenUrl = "https://manga.bilibili.com/twirp/comic.v1.Comic/ImageToken?device=pc&platform=web";
    auto postData = R"({"urls":"[\")" + imgRqstPaths[finishedImgCnt].toUtf8() + R"(\"]"})";
    httpReply = Network::Bili::postJson(getTokenUrl, postData);

    connect(httpReply, &QNetworkReply::finished, this, &ComicDownloadTask::getImgTokenFinished);
}

void ComicDownloadTask::getImgTokenFinished()
{
    auto data = getReplyJson("data");
    if (data.isNull() || data.isUndefined()) {
        return;
    }
    auto obj = data.toArray().first();
    auto token = obj["token"].toString();
    auto url = obj["url"].toString();
    auto index = Utils::paddedNum(finishedImgCnt + 1, Utils::numberOfDigit(totalImgCnt));
    auto fileName = index + Utils::fileExtension(url);
    file = openFileForWrite(fileName);
    if (!file) {
        return;
    }
    httpReply = Network::Bili::get(url + "?token=" + token);
    connect(httpReply, &QNetworkReply::readyRead, this, &ComicDownloadTask::onImgReadyRead);
    connect(httpReply, &QNetworkReply::finished, this, &ComicDownloadTask::downloadImgFinished);
}

std::unique_ptr<QSaveFile> ComicDownloadTask::openFileForWrite(const QString &fileName)
{
    if (!QFileInfo::exists(path)) {
        if (!QDir().mkpath(path)) {
            emit errorOccurred("创建目录失败");
            return nullptr;
        }
    }

    auto f = std::make_unique<QSaveFile>(QDir(path).filePath(fileName));
    if (!f->open(QIODevice::WriteOnly)) {
        emit errorOccurred("打开文件失败");
        return nullptr;
    }

    return f;
}

void ComicDownloadTask::onImgReadyRead()
{
    if (curImgTotalBytesCnt == 0) {
        curImgRecvBytesCnt = httpReply->header(QNetworkRequest::ContentLengthHeader).toLongLong();
    }
    auto size = httpReply->bytesAvailable();
    if (-1 == file->write(httpReply->readAll())) {
        emit errorOccurred("文件写入失败: " + file->errorString());
        abortCurrentImg();
        httpReply->abort();
    } else {
        curImgRecvBytesCnt += size;
    }
}

void ComicDownloadTask::abortCurrentImg()
{
    curImgRecvBytesCnt = 0;
    curImgTotalBytesCnt = 0;
    file.reset();
}

void ComicDownloadTask::downloadImgFinished()
{
    auto imgSize = curImgRecvBytesCnt;
    curImgRecvBytesCnt = 0;
    curImgTotalBytesCnt = 0;

    auto httpReply = this->httpReply;
    this->httpReply->deleteLater();
    this->httpReply = nullptr;

    auto file = std::move(this->file);

    auto error = httpReply->error();
    if (error != QNetworkReply::NoError) {
        if (error != QNetworkReply::OperationCanceledError) {
            emit errorOccurred("网络错误");
        }
        return;
    }

    if (!file->commit()) {
        emit errorOccurred("保存文件失败");
        return;
    }

    finishedImgCnt++;
    bytesCntTillLastImg += imgSize;
    downloadNextImg();
}

qint64 ComicDownloadTask::getDownloadedBytesCnt() const
{
    return bytesCntTillLastImg + curImgRecvBytesCnt;
}

int ComicDownloadTask::estimateRemainingSeconds(qint64 downBytesPerSec) const
{
    if (downBytesPerSec == 0) {
        return Unknown;
    }

    if (finishedImgCnt == totalImgCnt - 1 && curImgTotalBytesCnt != 0) {
        // last image, remaining bytes count is known
        return (curImgTotalBytesCnt - curImgRecvBytesCnt) / downBytesPerSec;
    } else if (finishedImgCnt == 0) {
        if (curImgTotalBytesCnt == 0) {
            return Unknown;
        } else {
            return (curImgTotalBytesCnt * totalImgCnt - curImgRecvBytesCnt) / downBytesPerSec;
        }
    } else {
        int estimateTotalBytes = bytesCntTillLastImg * totalImgCnt / finishedImgCnt;
        return (estimateTotalBytes - bytesCntTillLastImg - curImgRecvBytesCnt) / downBytesPerSec;
    }
}

double ComicDownloadTask::getProgress() const
{
    if (totalImgCnt == 0) {
        // new created. total Image count unknown
        return 0;
    }

    return static_cast<double>(finishedImgCnt) / totalImgCnt;
//    double progress = static_cast<double>(finishedImgCnt * 100) / static_cast<double>(totalImgCnt);
//    if (curImgTotalBytesCnt != 0) {
//        int estimateTotalBytes = totalImgCnt * curImgTotalBytesCnt;
//        progress += static_cast<double>(curImgRecvBytesCnt * 100) / static_cast<double>(estimateTotalBytes);
//    }
//    return static_cast<int>(progress);
}

QString ComicDownloadTask::getProgressStr() const
{
    if (totalImgCnt == 0) {
        return QString();
    } else {
        return QStringLiteral("%1页/%2页").arg(finishedImgCnt).arg(totalImgCnt);
    }
}

QString ComicDownloadTask::getQnDescription() const
{
    return QString();
}
