#include <cmath>

#include <QRegularExpression>

#include "app/utilities/stylesheet.hpp"

QString scale_stylesheet(QString stylesheet, double scale)
{
    static const QRegularExpression length(" (-?\\d+)px");

    QString scaled;
    qsizetype last = 0;
    auto it = length.globalMatch(stylesheet);
    while (it.hasNext()) {
        auto match = it.next();
        scaled += QStringView(stylesheet).mid(last, match.capturedStart(1) - last);
        scaled += QString::number(static_cast<int>(std::ceil(match.captured(1).toInt() * scale)));
        last = match.capturedEnd(1);
    }
    scaled += QStringView(stylesheet).mid(last);

    return scaled;
}
