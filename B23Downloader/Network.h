#ifndef NETWORK_H
#define NETWORK_H

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonObject>

#include <tuple>

namespace Network
{

QNetworkAccessManager *accessManager();

QNetworkReply *get(const QString &url);
QNetworkReply *get(const QUrl &url);

/**
 * @brief post data to url with content-type header set to application/x-www-form-urlencoded
 */
QNetworkReply *postUrlEncoded(const QString &url, const QByteArray &data);

/**
 * @brief post data to url with content-type header set to application/json
 */
QNetworkReply *postJson(const QString &url, const QByteArray &data);

/**
 * @brief post JsonObj to url (content-type header set to application/json)
 */
QNetworkReply *postJson(const QString &url, const QJsonObject &obj);


extern const QByteArray UserAgent;
extern const QByteArray Referer;

/**
 * @brief simple subclass of QNetworkRequest that sets referer and user-agent header in ctor
 */
class Request : public QNetworkRequest {
public:
    Request(const QUrl &url);
};


int statusCode(QNetworkReply *reply);


/*!
 * @return pair (json object, error string) (error string isNull if no error).
 * @param requiredKey is only used to help checking whether request is succeed.
 */
std::pair<QJsonObject, QString> parseReply(QNetworkReply *reply, const QString& requiredKey = QString());

} // namespace Network


#endif // NETWORK_H
