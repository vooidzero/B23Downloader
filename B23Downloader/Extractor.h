#ifndef EXTRACTOR_H
#define EXTRACTOR_H

#include <QObject>
#include <utility>

class QNetworkReply;

namespace ContentItemFlag
{
constexpr int NoFlags = 0;
constexpr int Disabled = 1;
constexpr int VipOnly = 2;
constexpr int PayOnly = 4;
constexpr int AllowWaitFree = 8; // manga

}

// UGC (User Generated Content): 普通视频
// PGC (Professional Generated Content): 剧集（番剧、电影、纪录片等）
// PUGV (Professional User Generated Video): 课程
enum class ContentType { UGC = 1, PGC, PUGV, Live, Comic };


// extract videos (title and cid) in url
class Extractor : public QObject
{
    Q_OBJECT

public:
    struct Result
    {
        ContentType type;
        qint64 id;
        QString title;
        qint64 focusItemId = 0;
        Result(ContentType type, qint64 id, const QString &title)
            : type(type), id(id), title(title) {}
        virtual ~Result() = default;
    };

    struct LiveResult: Result
    {
        LiveResult(qint64 roomId, const QString &title)
            : Result(ContentType::Live, roomId, title) {}
    };

    struct ContentItem
    {
        qint64 id;
        QString title;
        qint32 durationInSec;
        int flags;
        ContentItem(qint64 id, const QString &title, qint32 dur, int flags=ContentItemFlag::NoFlags)
            : id(id), title(title), durationInSec(dur), flags(flags) {}
    };

    struct Section
    {
        QString title;
        // qint64 id;
        QList<ContentItem> episodes;
        Section() {}
        Section(const QString &title): title(title) {}
    };

    struct ItemListResult: Result
    {
        QList<ContentItem> items;
        using Result::Result;
    };

    struct SectionListResult: Result
    {
        QList<Section> sections;
        using Result::Result;
    };

    enum class PgcIdType { SeasonId, EpisodeId };
    using PugvIdType = PgcIdType;

    Extractor();
    ~Extractor();
    void start(QString url);
    void abort();
    std::unique_ptr<Result> getResult();

signals:
    void errorOccurred(const QString &errorString);
    void success();

private:
    qint64 focusItemId = 0;
    std::unique_ptr<Result> result;
    QNetworkReply *httpReply = nullptr;
    QJsonObject getReplyJsonObj(const QString &requiredKey = QString());
    QString getReplyText();

    void parseUrl(QUrl url);
    void tryRedirect(const QUrl &url);
    void startUgc(const QString &query);
    void startUgcByBvId(const QString &bvid);
    void startUgcByAvId(qint64 avid);
    void startPgcByMdId(qint64 mdId);
    void startPgc(PgcIdType idType, qint64 id);
    void startPugv(PugvIdType idType, qint64 id);
    void startLive(qint64 roomId);
    void startLiveActivity(const QUrl &url);
    void startComic(int comicId);

    void urlNotSupported();

private slots:
    void ugcFinished();
    void pgcFinished();
    void pugvFinished();
    void liveFinished();
    void liveActivityFinished();
    void comicFinished();
};

#endif // EXTRACTOR_H
