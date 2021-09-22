#ifndef SETTINGS_H
#define SETTINGS_H

#include <QNetworkCookieJar>
#include <QNetworkAccessManager>
#include <QSettings>

#include <QColor>
namespace B23Style {
    constexpr QColor Pink(251, 114, 153);
    constexpr QColor Blue(0, 161, 214);
}

class CookieJar: public QNetworkCookieJar {
    static constexpr auto CookiesSeparator = '\n';

public:
    CookieJar(QObject *parent = nullptr);
    CookieJar(const QString &cookies, QObject *parent = nullptr);
    QByteArray getCookie(const QString &name) const;
    bool isEmpty() const;
    void clear();
    QString toString() const;

private:
    void fromString(const QString &cookies);
};

class Settings: public QSettings
{
    CookieJar *cookieJar;

public:
    Settings(QObject *parent = nullptr);
    static Settings *inst();

    CookieJar *getCookieJar();
    bool hasCookies();
    void saveCookies();
    void removeCookies();
};

#endif // SETTINGS_H
