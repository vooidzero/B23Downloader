#include "Settings.h"
#include <QtNetwork>

CookieJar::CookieJar(QObject *parent)
  : QNetworkCookieJar(parent)
{}

CookieJar::CookieJar(const QString &cookies, QObject *parent)
  : QNetworkCookieJar(parent)
{
    fromString(cookies);
}

bool CookieJar::isEmpty() const
{
    return allCookies().isEmpty();
}

void CookieJar::clear()
{
    setAllCookies(QList<QNetworkCookie>());
}

QByteArray CookieJar::getCookie(const QString &name) const
{
    for (auto &cookie : allCookies()) {
        if (cookie.name() == name) {
            return cookie.value();
        }
    }
    return QByteArray();
}

QString CookieJar::toString() const
{
    QString ret;
    for (auto &cookie : allCookies()) {
        if (!ret.isEmpty()) {
            ret.append(CookiesSeparator);
        }
        ret.append(cookie.toRawForm());
    }
    return ret;
}

void CookieJar::fromString(const QString &data)
{
    QList<QNetworkCookie> cookies;
    auto cookieStrings = data.split(CookiesSeparator);
    for (auto &cookieStr : cookieStrings) {
        cookies.append(QNetworkCookie::parseCookies(cookieStr.toUtf8()));
    }
    setAllCookies(cookies);
}


Q_GLOBAL_STATIC(Settings, settings)

static constexpr auto KeyCookies = "cookies";

Settings::Settings(QObject *parent)
    :QSettings(QSettings::IniFormat, QSettings::UserScope, "VoidZero", "B23Downloader", parent)
{
    setFallbacksEnabled(false);
    auto cookiesStr = value(KeyCookies).toString();
    cookieJar = new CookieJar(cookiesStr, parent);
    if (!cookiesStr.isEmpty() && cookieJar->isEmpty()) {
        remove(KeyCookies);
    }
}

Settings *Settings::inst()
{
    return settings();
}

CookieJar *Settings::getCookieJar()
{
    return cookieJar;
}

bool Settings::hasCookies()
{
    return contains(KeyCookies); // !cookieJar->isEmpty();
}

void Settings::saveCookies()
{
    setValue(KeyCookies, cookieJar->toString());
}

void Settings::removeCookies()
{
    cookieJar->clear();
    remove(KeyCookies);
}
