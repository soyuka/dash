#include <QRegularExpression>

#include "app/launch_options.hpp"

namespace {
QSize parse_size(const QString &value)
{
    auto match = QRegularExpression("^(\\d+)x(\\d+)$").match(value);
    if (!match.hasMatch())
        return QSize();
    QSize size(match.captured(1).toInt(), match.captured(2).toInt());
    return size.isEmpty() ? QSize() : size;
}
}

LaunchOptions::LaunchOptions()
    : windowed("windowed", "Run in a WIDTHxHEIGHT window instead of fullscreen.", "WIDTHxHEIGHT")
    , logical("logical-size", "Lay out at WIDTHxHEIGHT and stretch to fill the window.", "WIDTHxHEIGHT")
{
}

void LaunchOptions::add_to(QCommandLineParser &parser) const
{
    parser.addOption(this->windowed);
    parser.addOption(this->logical);
}

void LaunchOptions::resolve(const QCommandLineParser &parser, const QSettings &settings)
{
    auto read = [this](const QString &name, const QString &value) {
        auto size = parse_size(value);
        if (!size.isValid())
            this->error = QString("%1 expects WIDTHxHEIGHT, got %2").arg(name, value);
        return size;
    };
    auto read_setting = [&](const QString &key) {
        auto value = settings.value(key);
        if (value.typeId() == QMetaType::QSize)
            return value.toSize().isEmpty() ? read(key, "0x0") : value.toSize();
        return read(key, value.toString());
    };

    if (parser.isSet(this->windowed))
        this->window_size = read("--windowed", parser.value(this->windowed));
    else if (settings.contains("Window/size"))
        this->window_size = settings.value("Window/size").toSize();

    if (parser.isSet(this->logical))
        this->logical_size = read("--logical-size", parser.value(this->logical));
    else if (settings.contains("Window/logical_size"))
        this->logical_size = read_setting("Window/logical_size");
}
