#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>
#include <QScreen>
#include <QRegularExpression>
#include <QSettings>

#include "DashLog.hpp"
#include "app/action.hpp"
#include "app/window.hpp"

int main(int argc, char *argv[])
{
    QApplication dash(argc, argv);
    dash.setOrganizationName("openDsh");
    dash.setApplicationName("dash");
    // Wayland app_id, for compositor window rules (e.g. labwc <windowRule identifier="dash">)
    dash.setDesktopFileName("dash");
    dash.setWindowIcon(QIcon(":/logo.png"));
    dash.installEventFilter(ActionEventFilter::get_instance());

    QCommandLineParser parser;
    parser.setApplicationDescription("openDsh head unit");
    parser.addHelpOption();
    QCommandLineOption windowed("windowed", "Run in a WIDTHxHEIGHT window instead of fullscreen.", "WIDTHxHEIGHT");
    parser.addOption(windowed);
    parser.process(dash);

    QSettings settings;
    DASH_LOG(info) << "loaded config: " << settings.fileName().toStdString();

    QSize size;
    if (parser.isSet(windowed)) {
        auto match = QRegularExpression("^(\\d+)x(\\d+)$").match(parser.value(windowed));
        if (!match.hasMatch()) {
            DASH_LOG(error) << "--windowed expects WIDTHxHEIGHT, got " << parser.value(windowed).toStdString();
            return 1;
        }
        size = QSize(match.captured(1).toInt(), match.captured(2).toInt());
    }
    else if (settings.contains("Window/size")) {
        size = settings.value("Window/size").toSize();
    }

    MainWindow window;
    if (size.isValid()) {
        window.resize(size);
        window.show();
    }
    else {
        // Without a size of its own the window is shown at its size hint, and Qt
        // Wayland applies that after the compositor's fullscreen configure.
        window.resize(window.screen()->size());
        window.showFullScreen();
    }

    return dash.exec();
}
