// Created by voidzero <vooidzero.github@qq.com>

#include "utils.h"
#include "Extractor.h"
#include "Network.h"
#include <QtNetwork>

using QRegExp = QRegularExpression;
using std::make_unique;
using std::move;


Extractor::Extractor()
{
    focusItemId = 0;
}

Extractor::~Extractor()
{
    if (httpReply != nullptr) {
        httpReply->abort();
    }
}

void Extractor::urlNotSupported()
{
    static const QString supportedUrlTip =
        "输入错误或不支持. 支持的输入有: <ul>"
        "<li>B站 剧集/视频/直播/课程/漫画 链接</li>"
        "<li>剧集(番剧,电影等)ss或ep号, 比如《招魂》: <em>ss28341</em> 或 <em>ep281280</em></li>"
        "<li>视频BV或av号, 比如: <em>BV1A2b3X4y5Z</em> 或 <em>av123456</em></li>"
        "<li>live+房间号, 比如LOL赛事直播: <em>live6</em></li>"
        "</ul>"
        "<p align=\"right\">by: <a href=\"http://github.com/vooidzero/B23Downloader\">github.com/vooidzero</a></p>";
    emit errorOccurred(supportedUrlTip);
}

void Extractor::start(QString url)
{
    url = url.trimmed();
    QRegularExpressionMatch m;

    // bad coding style?!

    // try to match short forms
    if ((m = QRegExp("^(?:BV|bv)([a-zA-Z0-9]+)$").match(url)).hasMatch()) {
        return startUgcByBvId("BV" + m.captured(1));
    }
    if ((m = QRegExp(R"(^av(\d+)$)").match(url)).hasMatch()) {
        return startUgcByAvId(m.captured(1).toLongLong());
    }
    if ((m = QRegExp(R"(^(ss|ep)(\d+)$)").match(url)).hasMatch()) {
        auto idType = (m.captured(1) == "ss" ? PgcIdType::SeasonId : PgcIdType::EpisodeId);
        return startPgc(idType, m.captured(2).toLongLong());
    }
    if ((m = QRegExp(R"(^live(\d+)$)").match(url)).hasMatch()) {
        return startLive(m.captured(1).toLongLong());
    }

    if (!url.startsWith("http://") && !url.startsWith("https://")) {
        parseUrl("https://" + url);
    } else {
        parseUrl(url);
    }
}

void Extractor::parseUrl(QUrl url)
{
    auto host = url.authority().toLower();
    auto path = url.path();
    auto query = QUrlQuery(url);
    QRegularExpressionMatch m;

    if (QRegExp(R"(^(?:www\.|m\.)?bilibili\.com$)").match(host).hasMatch()) {
        if ((m = QRegExp(R"(^/bangumi/play/(ss|ep)(\d+)/?$)").match(path)).hasMatch()) {
            auto idType = (m.captured(1) == "ss" ? PgcIdType::SeasonId : PgcIdType::EpisodeId);
            return startPgc(idType, m.captured(2).toLongLong());
        }
        if ((m = QRegExp(R"(^/bangumi/media/md(\d+)/?$)").match(path)).hasMatch()) {
            return startPgcByMdId(m.captured(1).toLongLong());
        }
        if ((m = QRegExp(R"(^/cheese/play/(ss|ep)(\d+)/?$)").match(path)).hasMatch()) {
            auto idType = (m.captured(1) == "ss" ? PugvIdType::SeasonId : PugvIdType::EpisodeId);
            return startPugv(idType, m.captured(2).toLongLong());
        }

        focusItemId = query.queryItemValue("p").toLongLong();
        if ((m = QRegExp(R"(^/(?:(?:s/)?video/)?(?:BV|bv)([a-zA-Z0-9]+)/?$)").match(path)).hasMatch()) {
            return startUgcByBvId("BV" + m.captured(1));
        }
        if ((m = QRegExp(R"(^/(?:(?:s/)?video/)?av(\d+)/?$)").match(path)).hasMatch()) {
            return startUgcByAvId(m.captured(1).toLongLong());
        }
//        if ((m = QRegExp(R"(^$)").match(path)).hasMatch()) {
//        }
        return urlNotSupported();
    }

    if (host == "bangumi.bilibili.com") {
        if ((m = QRegExp(R"(^/anime/(\d+)/?$)").match(path)).hasMatch()) {
            return startPgc(PgcIdType::SeasonId, m.captured(1).toLongLong());
        }
        return urlNotSupported();
    }

    if (host == "live.bilibili.com") {
        if ((m = QRegExp(R"(^/(?:h5/)?(\d+)/?$)").match(path)).hasMatch()) {
            return startLive(m.captured(1).toLongLong());
        }
        if ((m = QRegExp(R"(^/blackboard/activity-.*\.html$)").match(path)).hasMatch()) {
            return startLiveActivity(url);
        }

        return urlNotSupported();
    }

    if (host == "b23.tv") {
        if ((m = QRegExp(R"(^/(ss|ep)(\d+)$)").match(path)).hasMatch()) {
            auto idType = (m.captured(1) == "ss" ? PugvIdType::SeasonId : PugvIdType::EpisodeId);
            return startPgc(idType, m.captured(2).toLongLong());
        } else {
            return tryRedirect(url);
        }
    }

    if (host == "manga.bilibili.com") {
        if ((m = QRegExp(R"(^/(?:m/)?detail/mc(\d+)/?$)").match(path)).hasMatch()) {
            focusItemId = query.queryItemValue("epId").toLongLong();
            return startComic(m.captured(1).toLongLong());
        }
        if ((m = QRegExp(R"(^/(?:m/)?mc(\d+)/(\d+)/?$)").match(path)).hasMatch()) {
            focusItemId = m.captured(2).toLongLong();
            return startComic(m.captured(1).toLongLong());
        }
        return urlNotSupported();
    }

    if (host == "b22.top") {
        return tryRedirect(url);
    }

//    auto tenkinokoWebAct = "www.bilibili.com/blackboard/topic/activity-jjR1nNRUF.html";
//    auto tenkinokoMobiAct = "www.bilibili.com/blackboard/topic/activity-4AL5_Jqb3";

    return urlNotSupported();
}

void Extractor::abort()
{
    if (httpReply != nullptr) {
        httpReply->abort();
    }
}

std::unique_ptr<Extractor::Result> Extractor::getResult()
{
    result->focusItemId = this->focusItemId;
    return move(result);
}


QJsonObject Extractor::getReplyJsonObj(const QString &requiredKey)
{
    auto reply = this->httpReply;
    this->httpReply = nullptr;
    reply->deleteLater();
    if (reply->error() == QNetworkReply::OperationCanceledError) {
        return QJsonObject();
    }

    const auto [json, errorString] = Network::Bili::parseReply(reply, requiredKey);

    if (!errorString.isNull()) {
        emit errorOccurred(errorString);
        return QJsonObject();
    }


    if (requiredKey.isEmpty()) {
        return json;
    } else {
        auto ret = json[requiredKey].toObject();
        if (ret.isEmpty()) {
            emit errorOccurred("请求错误: 内容为空");
            return QJsonObject();
        }
        return ret;
    }
}

QString Extractor::getReplyText()
{
    auto reply = httpReply;
    httpReply = nullptr;
    reply->deleteLater();
    if (reply->error() == QNetworkReply::OperationCanceledError) {
        return QString();
    }
    if (reply->error() != QNetworkReply::NoError) {
        emit errorOccurred("网络请求错误");
        return QString();
    }

    return QString::fromUtf8(reply->readAll());
}

void Extractor::tryRedirect(const QUrl &url)
{
    auto rqst = QNetworkRequest(url);
    rqst.setMaximumRedirectsAllowed(0);
    httpReply = Network::accessManager()->get(rqst);
    connect(httpReply, &QNetworkReply::finished, this, [this]{
        auto reply = httpReply;
        httpReply = nullptr;
        reply->deleteLater();
        if (reply->hasRawHeader("Location")) {
            auto redirect = QString::fromUtf8(reply->rawHeader("Location"));
            if (redirect.contains("bilibili.com")) {
                parseUrl(redirect);
            } else {
                emit errorOccurred("重定向目标非B站");
            }
        } else if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred("网络错误");
        } else {
            emit errorOccurred("未知错误");
        }
    });
}


void Extractor::startPgcByMdId(qint64 mdId)
{
    auto query = "https://api.bilibili.com/pgc/review/user?media_id=" + QString::number(mdId);
    httpReply = Network::Bili::get(query);
    connect(httpReply, &QNetworkReply::finished, this, [this] {
        auto result = getReplyJsonObj("result");
        if (result.isEmpty()) {
            return;
        }
        auto ssid = result["media"].toObject()["season_id"].toInteger();
        startPgc(PgcIdType::SeasonId, ssid);
    });
}

void Extractor::startPgc(PgcIdType idType, qint64 id)
{
    if (idType == PgcIdType::EpisodeId) {
        focusItemId = id;
    }
    auto api = "https://api.bilibili.com/pgc/view/web/season";
    auto query = QString("?%1=%2").arg(idType == PgcIdType::SeasonId ? "season_id" : "ep_id").arg(id);
    httpReply = Network::Bili::get(api + query);
    connect(httpReply, &QNetworkReply::finished, this, &Extractor::pgcFinished);
}

static int epFlags(int epStatus)
{
    switch (epStatus) {
    case 2:
        return ContentItemFlag::NoFlags;
    case 13:
        return ContentItemFlag::VipOnly;
    case 6: case 7:
    case 8: case 9:
    case 12:
        return ContentItemFlag::PayOnly;
    // case 14: 限定 https://www.bilibili.com/bangumi/media/md28234679
    default:
        qDebug() << "unknown ep status" << epStatus;
        return ContentItemFlag::NoFlags;
    }
}

static QString epIndexedTitle(const QString &title, int fieldWidth, const QString &indexSuffix)
{
    bool isNum;
    title.toDouble(&isNum);
    if (!isNum) {
        return title;
    }

    auto &unpadNumStr = title;
    auto dotPos = unpadNumStr.indexOf('.'); // in case of float number like 34.5
    auto padLen = fieldWidth - (dotPos == -1 ? unpadNumStr.size() : dotPos);
    return QString("第%1%2").arg(QString(padLen, '0') + unpadNumStr, indexSuffix);
}


static QString epTitle (const QString &title, const QString &longTitle) {
    if (longTitle.isEmpty()) {
        return title;
    } else {
        return title + " " + longTitle;
    }
}

void Extractor::pgcFinished()
{
    auto res = getReplyJsonObj("result");
    if (res.isEmpty()) {
        return;
    }

    auto type = res["type"].toInt();
    QString indexSuffix = (type == 1 || type == 4) ? "话" : "集";

    auto ssid = res["season_id"].toInteger();
    auto title = res["title"].toString();
    auto pgcRes = make_unique<SectionListResult>(ContentType::PGC, ssid, title);
    auto mainSecEps = res["episodes"].toArray();
    auto totalEps = res["total"].toInt();
    if (totalEps <= 0) {
        totalEps = mainSecEps.size();
    }
    auto indexFieldWidth = QString::number(totalEps).size();
    pgcRes->sections.emplaceBack("正片");
    auto &mainSection = pgcRes->sections.first();
    for (auto epValR : mainSecEps) {
        auto epObj = epValR.toObject();
        auto title = epObj["title"].toString();
        auto longTitle = epObj["long_title"].toString();
        mainSection.episodes.emplaceBack(
            epObj["id"].toInteger(),
            epTitle(epIndexedTitle(title, indexFieldWidth, indexSuffix), longTitle),
            qRound(epObj["duration"].toDouble() / 1000.0),
            epFlags(epObj["status"].toInt())
        );
    }
    if (focusItemId == 0 && mainSection.episodes.size() == 1) {
        focusItemId = mainSection.episodes.first().id;
    }

    for (auto &&secValR : res["section"].toArray()) {
        auto secObj = secValR.toObject();
        auto eps = secObj["episodes"].toArray();
        if (eps.size() == 0) {
            continue;
        }
        pgcRes->sections.emplaceBack(secObj["title"].toString());
        auto &sec = pgcRes->sections.last();
        for (auto &&epValR : eps) {
            auto epObj = epValR.toObject();
            sec.episodes.emplaceBack(
                epObj["id"].toInteger(),
                epTitle(epObj["title"].toString(), epObj["long_title"].toString()),
                qRound(epObj["duration"].toDouble() / 1000.0),
                epFlags(epObj["status"].toInt())
            );
        }
    }

    // season is free
    if (res["status"].toInt() == 2) {
        this->result = move(pgcRes);
        emit success();
        return;
    }

    // season is not free. add user payment info
    auto userStatApi = "https://api.bilibili.com/pgc/view/web/season/user/status";
    auto query = "?season_id=" + QString::number(ssid);
    httpReply = Network::Bili::get(userStatApi + query);
    connect(httpReply, &QNetworkReply::finished, this, [this, pgcRes = move(pgcRes)]() mutable {
        auto result = getReplyJsonObj("result");
        if (result.isEmpty()) {
            return;
        }
        auto userIsVip = (result["vip_info"].toObject()["status"].toInt() == 1);
        auto userHasPaid = (result["pay"].toInt(1) == 1);
        if (!userHasPaid) {
            for (auto &video : pgcRes->sections.first().episodes) {
                auto notFree = (video.flags & ContentItemFlag::VipOnly) or (video.flags & ContentItemFlag::PayOnly);
                if (notFree && !userHasPaid) {
                    video.flags |= ContentItemFlag::Disabled;
                }
            }
        }

        this->result = move(pgcRes);
        emit success();
    });
}


void Extractor::startLive(qint64 roomId)
{
    auto api = "https://api.live.bilibili.com/xlive/web-room/v1/index/getInfoByRoom";
    auto query = "?room_id=" + QString::number(roomId);
    httpReply = Network::Bili::get(api + query);
    connect(httpReply, &QNetworkReply::finished, this, &Extractor::liveFinished);
}

void Extractor::liveFinished()
{
    auto json = getReplyJsonObj();
    if (json.isEmpty()) {
        return;
    }

    if (!json.contains("data") || json["data"].type() == QJsonValue::Null) {
        emit errorOccurred("B站请求错误: 非法房间号");
        return;
    }

    auto data = json["data"].toObject();
    auto roomInfo = data["room_info"].toObject();
    if (!roomInfo.contains("live_status")) {
        emit errorOccurred("发生了错误: getInfoByRoom: 未找到live_status");
        return;
    }
    auto roomStatus = roomInfo["live_status"].toInt();
    if (roomStatus == 0) {
        emit errorOccurred("该房间当前未开播");
        return;
    }
    if (roomStatus == 2) {
        emit errorOccurred("该房间正在轮播.");
        return;
    }

    auto roomId = roomInfo["room_id"].toInteger();
    bool hasPayment = (roomInfo["special_type"].toInt() == 1);
    auto title = roomInfo["title"].toString();
    auto uname = data["anchor_info"].toObject()["base_info"].toObject()["uname"].toString();

    auto liveRes = make_unique<LiveResult>(roomId, QString("【%1】%2").arg(uname, title));
    if (!hasPayment) {
        this->result = move(liveRes);
        emit success();
        return;
    }

    auto validateApi = "https://api.live.bilibili.com/av/v1/PayLive/liveValidate";
    auto query = "?room_id=" + QString::number(roomId);
    httpReply = Network::Bili::get(validateApi + query);
    connect(httpReply, &QNetworkReply::finished, this, [this, liveRes = move(liveRes)]() mutable {
        auto data = getReplyJsonObj("data");
        if (data.isEmpty()) {
            return;
        }

        if (data["permission"].toInt()) {
            this->result = move(liveRes);
            emit success();
        } else {
            emit errorOccurred("该直播需要付费购票观看");
        }
    });
}

void Extractor::startLiveActivity(const QUrl &url)
{
    httpReply = Network::Bili::get(url);
    connect(httpReply, &QNetworkReply::finished, this, &Extractor::liveActivityFinished);
}

void Extractor::liveActivityFinished()
{
    auto text = getReplyText();
    if (text.isNull()) {
        return;
    }

    auto parseFailed = [this]{
        emit errorOccurred("解析活动页面失败。<br>建议尝试数字房间号链接, 比如<em>live.bilibili.com/22586886</em>");
    };

    auto m = QRegExp(R"(window.__BILIACT_ENV__\s?=([^;]+);)").match(text);
    auto platform = m.captured(1); // null if no match
    if (platform.contains("H5")) {
        m = QRegExp(R"(\\?"jumpUrl\\?"\s?:\s?\\?"([^"\\]+)\\?")").match(text, m.capturedEnd(0));
        if (m.hasMatch()) {
            startLiveActivity(m.captured(1));
        } else {
            parseFailed();
        }
        return;
    }

    m = QRegExp(R"(\\?"defaultRoomId\\?"\s?:\s?\\?"?(\d+))").match(text, m.capturedEnd(0));
    if (!m.hasMatch()) {
        parseFailed();
        return;
    }
    startLive(m.captured(1).toLongLong());
}



void Extractor::startPugv(PugvIdType idType, qint64 id)
{
    if (idType == PgcIdType::EpisodeId) {
        focusItemId = id;
    }
    auto api = "https://api.bilibili.com/pugv/view/web/season";
    auto query = QString("?%1=%2").arg(idType == PugvIdType::SeasonId ? "season_id" : "ep_id").arg(id);
    httpReply = Network::Bili::get(api + query);
    connect(httpReply, &QNetworkReply::finished, this, &Extractor::pugvFinished);
}

void Extractor::pugvFinished()
{
    auto data = getReplyJsonObj("data");
    if (data.isEmpty()) {
        return;
    }

    if (!data["user_status"].toObject()["payed"].toInt(1)) {
        emit errorOccurred("未登录或未购买该课程");
        return;
    }

    auto ssid = data["season_id"].toInteger();
    auto title = data["title"].toString();
    ItemListResult(ContentType::PUGV, ssid, title);
    auto pugvRes = make_unique<ItemListResult>(ContentType::PUGV, ssid, title);
    for (auto &&epValR : data["episodes"].toArray()) {
        auto epObj = epValR.toObject();
        pugvRes->items.emplaceBack(
            epObj["id"].toInteger(),
            epObj["title"].toString(),
            epObj["duration"].toInt()
        );
    }

    this->result = move(pugvRes);
    emit success();
}


void Extractor::startUgcByBvId(const QString &bvid)
{
    startUgc("bvid=" + bvid);
}

void Extractor::startUgcByAvId(qint64 avid)
{
    startUgc("aid=" + QString::number(avid));
}

void Extractor::startUgc(const QString &query)
{
    httpReply = Network::Bili::get("https://api.bilibili.com/x/web-interface/view?" + query);
    connect(httpReply, &QNetworkReply::finished, this, &Extractor::ugcFinished);
}

void Extractor::ugcFinished()
{
    auto data = getReplyJsonObj("data");
    if (data.isEmpty()) {
        return;
    }

    auto isInteractVideo = data["rights"].toObject()["is_stein_gate"].toInt();
    if (isInteractVideo) {
        emit errorOccurred("不支持互动视频");
        return;
    }

    auto redirect = data["redirect_url"].toString();
    if (!redirect.isNull()) {
        parseUrl(redirect);
        return;
    }

    auto pages = data["pages"].toArray();
    if (pages.isEmpty()) {
        emit errorOccurred(data.isEmpty() ? "发生了错误: getUgcInfo: data为空" : "什么都没有...");
        return;
    }

    auto avid = data["aid"].toInteger();
    auto title = data["title"].toString();
    auto ugcRes = make_unique<ItemListResult>(ContentType::UGC, avid, title);
    for (auto &&pageValR : pages) {
        auto pageObj = pageValR.toObject();
        ugcRes->items.emplaceBack(
            pageObj["cid"].toInteger(),
            pageObj["part"].toString(),
            pageObj["duration"].toInt()
        );
    }

    auto index = focusItemId;
    if (index > 0 && index <= ugcRes->items.size()) {
        focusItemId = ugcRes->items[index - 1].id;
    } else if (ugcRes->items.size() == 1) {
        focusItemId = ugcRes->items.first().id;
    } else {
        focusItemId = 0;
    }
    this->result = move(ugcRes);
    emit success();
}



void Extractor::startComic(int comicId)
{
    auto url = "https://manga.bilibili.com/twirp/comic.v1.Comic/ComicDetail?device=pc&platform=web";
    httpReply = Network::Bili::postJson(QString(url), QJsonObject{{"comic_id", comicId}});
    connect(httpReply, &QNetworkReply::finished, this, &Extractor::comicFinished);
}

enum ComicType { Comic, Video };

static QString comicEpTitle(const QString &shortTitle, const QString &title, int indexWidth)
{
    // 想优化一下标题。但情况太杂了，这个函数就简单写吧

    bool isNumber;
    int index = shortTitle.toInt(&isNumber);
    if (isNumber) {
        if (index == title.toInt(&isNumber) && isNumber) {
            return Utils::paddedNum(index, indexWidth);
        }
    }
    return (title.isEmpty() ? shortTitle : shortTitle + " " + title);
}

void Extractor::comicFinished()
{
    auto data = getReplyJsonObj("data");
    if (data.isEmpty()) {
        return;
    }

    if (data["type"].toInt() == ComicType::Video) {
        emit errorOccurred("暂不支持Vomic");
        return;
    }

    auto id = data["id"].toInteger();
    auto title = data["title"].toString();
    auto comicRes = make_unique<ItemListResult>(ContentType::Comic, id, title);
    auto epList = data["ep_list"].toArray();
    comicRes->items.reserve(epList.size());
    auto indexWidth = Utils::numberOfDigit(static_cast<int>(epList.size()));

    // Episodes in epList should be sorted by 'ord' (which may start from 0 or 1).
    // Though currently episodes in reply are already sorted in descending order,
    //     I think it's not good to rely on that.
    QMap<int, int> ordMap;
    for (int i = 0; i < epList.size(); i++) {
        auto ord = epList[i].toObject()["ord"].toInt();
        ordMap[ord] = i;
    }
    for (int idx : ordMap) {
        const auto epObj = epList[idx].toObject();
        auto flags = ContentItemFlag::NoFlags;
        if (epObj["is_locked"].toBool()) {
            flags |= ContentItemFlag::Disabled;
        }
        if (epObj["allow_wait_free"].toBool()) {
            flags |= ContentItemFlag::AllowWaitFree;
        } else if (epObj["pay_mode"].toInt(0) != 0) {
            flags |= ContentItemFlag::PayOnly;
        }

        auto title = comicEpTitle(
            epObj["short_title"].toString(),
            epObj["title"].toString(),
            indexWidth
        );

        auto epid = epObj["id"].toInteger();
        comicRes->items.emplaceBack(epid, title, 0, flags);
    }


    this->result = move(comicRes);
    emit success();
}
