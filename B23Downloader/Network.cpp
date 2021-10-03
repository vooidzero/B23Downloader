#include "Network.h"
#include <QtNetwork>

namespace Network
{

Q_GLOBAL_STATIC(QNetworkAccessManager, nam)

QNetworkAccessManager *accessManager()
{
    return nam();
}

int statusCode(QNetworkReply *reply)
{
    return reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
}



const QByteArray Bili::Referer("https://www.bilibili.com");
const QByteArray Bili::UserAgent("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/90.0.4430.85 Safari/537.36 Edg/90.0.818.49");

Bili::Request::Request(const QUrl &url)
    : QNetworkRequest(url)
{
    setMaximumRedirectsAllowed(0);
    setRawHeader("Referer", Bili::Referer);
    setRawHeader("User-Agent", Bili::UserAgent);
}


QNetworkReply *Bili::get(const QUrl &url)
{
    return nam->get(Bili::Request(url));
}

QNetworkReply *Bili::get(const QString &url)
{
    return nam->get(Bili::Request(url));
}

QNetworkReply *Bili::postUrlEncoded(const QString &url, const QByteArray &data)
{
    auto request = Bili::Request(url);
    request.setRawHeader("content-type", "application/x-www-form-urlencoded;charset=UTF-8");
    return nam->post(request, data);
}

QNetworkReply *Bili::postJson(const QString &url, const QByteArray &data)
{
    auto request = Bili::Request(url);
    request.setRawHeader("content-type", "application/json;charset=UTF-8");
    return nam->post(request, data);
}

QNetworkReply *Bili::postJson(const QString &url, const QJsonObject &obj)
{
    auto request = Bili::Request(url);
    request.setRawHeader("content-type", "application/json;charset=UTF-8");
    return nam->post(request, QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

static bool isJsonValueInvalid(const QJsonValue &val)
{
    return val.isNull() || val.isUndefined();
}

std::pair<QJsonObject, QString> Bili::parseReply(QNetworkReply *reply, const QString& requiredKey)
{
    if (reply->error() != QNetworkReply::NoError) {
        qDebug() << "network error:" << reply->errorString() << ", url=" << reply->url().toString();
        return { QJsonObject(), "网络请求错误" };
    }
    if (!reply->header(QNetworkRequest::ContentTypeHeader).toString().contains("json")) {
        return { QJsonObject(), "http请求错误" };
    }
    auto data = reply->readAll();
    auto jsonDoc = QJsonDocument::fromJson(data);
    auto jsonObj = jsonDoc.object();
    qDebug() << "reply from" << reply->url(); // << QString::fromUtf8(data);

    if (jsonObj.isEmpty()) {
        return { QJsonObject(), "http请求错误" };
    }

    int code = jsonObj["code"].toInt(0);
    if (code < 0 || (!requiredKey.isEmpty() && isJsonValueInvalid(jsonObj[requiredKey]))) {
        if (jsonObj.contains("message")) {
            return { jsonObj, jsonObj["message"].toString() };
        }
        if (jsonObj.contains("msg")) {
            return { jsonObj, jsonObj["msg"].toString() };
        }
        auto format = QStringLiteral("B站请求错误: code = %1, requiredKey = %2\nURL: %3");
        auto msg = format.arg(QString::number(code), requiredKey, reply->url().toString());
        return { jsonObj, msg };
    }

    return { jsonObj, QString() };
}


} // namespace Network
