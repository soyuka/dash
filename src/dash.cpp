#include <memory>

#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>
#include <QScreen>
#include <QSettings>

#include "DashLog.hpp"
#include "app/action.hpp"
#include "app/launch_options.hpp"
#include "app/stretch_view.hpp"
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
    LaunchOptions options;
    options.add_to(parser);
    parser.process(dash);

    QSettings settings;
    DASH_LOG(info) << "loaded config: " << settings.fileName().toStdString();

    options.resolve(parser, settings);
    if (!options.error.isEmpty()) {
        DASH_LOG(error) << options.error.toStdString();
        return 1;
    }

    auto window = new MainWindow();
    std::unique_ptr<QWidget> top_level;
    if (options.logical_size.isValid()) {
        DASH_LOG(info) << "stretching a " << options.logical_size.width() << "x" << options.logical_size.height()
                       << " layout to the window";
        top_level = std::make_unique<StretchView>(window, options.logical_size);
    }
    else {
        top_level.reset(window);
    }

    if (options.window_size.isValid()) {
        top_level->resize(options.window_size);
        top_level->show();
    }
    else {
        // Without a size of its own the window is shown at its size hint, and Qt
        // Wayland applies that after the compositor's fullscreen configure.
        top_level->resize(top_level->screen()->size());
        top_level->showFullScreen();
    }

    return dash.exec();
}
