#include <cmath>

#include <QRegularExpression>

#include "app/utilities/stylesheet.hpp"

QString scale_stylesheet(QString stylesheet, double scale)
{
    QRegularExpression regex(" (-?\\d+)px");

    auto it = regex.globalMatch(stylesheet);
    while (it.hasNext()) {
        auto match = it.next();
        if (match.hasMatch()) {
            int scaled_px = std::ceil(match.captured(1).toInt() * scale);
            stylesheet.replace(match.captured(), QString("%1px").arg(scaled_px));
        }
    }

    return stylesheet;
}
