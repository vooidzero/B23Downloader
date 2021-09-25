#ifndef UTILS_H
#define UTILS_H

#include <QLabel>

// instances should set fixed/hint/max width to work
class ElidedTextLabel : public QLabel
{
    Q_OBJECT

    QColor color;
    int hintWidth = 0;
    Qt::TextElideMode elideMode = Qt::ElideRight;

public:
    ElidedTextLabel(QWidget *parent = nullptr);
    ElidedTextLabel(const QString &text, QWidget *parent = nullptr);

    void setElideMode(Qt::TextElideMode mode);
    void setHintWidthToString(const QString &sample);
    void setFixedWidthToString(const QString &sample);

    // clear color and text
    void clear();

    // set text and text color; toolTip is set to be same as text
    // if arg 'color' is invalid (default val), text color is set to default (black)
    void setText(const QString &str, const QColor &color = QColor());

    // the color is set to red
    void setErrText(const QString &str);

protected:
    void paintEvent(QPaintEvent *event) override;
    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;
    bool event(QEvent *e) override;
};



namespace Utils
{
    // fileExtension("abc.txt") == ".txt"
    // fileExtension("abc") is Null
    QString fileExtension(const QString &fileName);

    int numberOfDigit(int num);

    // pad str(num) with leading 0 so that its length is equal to width
    QString paddedNum(int num, int width);

    // 用全角字符替换视频标题中的 \/:*?"<>| 从而确保文件名合法
    QString legalizedFileName(QString title);

    std::tuple<int, int, int> secs2HMS(int secs);

    QString secs2HmsStr(int secs);

    QString secs2HmsLocaleStr(int secs);

    QString formattedDataSize(qint64 bytes);

} // namespace Utils

#endif // UTILS_H
