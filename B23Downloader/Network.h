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

// `data` is url encoded
QNetworkReply *postUrlEncoded(const QString &url, const QByteArray &data);
QNetworkReply *postJson(const QString &url, const QByteArray &data);
QNetworkReply *postJson(const QString &url, const QJsonObject &obj);

class Request : public QNetworkRequest {
public:
    Request(const QUrl &url);
};


int statusCode(QNetworkReply *reply);


// @return json object, error string (isNull if no error)
std::pair<QJsonObject, QString> parseReply(QNetworkReply *reply, const QString& requiredKey = QString());

} // namespace Network


#endif // NETWORK_H
