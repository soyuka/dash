#include <algorithm>
#include <memory>

#include <QCoreApplication>
#include <QFile>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QSet>
#include <QSignalBlocker>
#include <QSize>
#include <QSlider>
#include <QTextStream>

#include "app/arbiter.hpp"
#include "app/pages/vehicle.hpp"
#include "app/pages/launcher.hpp"
#include "app/pages/media.hpp"
#include "app/pages/settings.hpp"
#include "app/quick_views/combo.hpp"
#include "app/utilities/icon_engine.hpp"
#include "app/utilities/stylesheet.hpp"
#include "plugins/brightness_plugin.hpp"
#include "aasdk_proto/ButtonCodeEnum.pb.h"
#include "DashLog.hpp"

#include "app/session.hpp"

QDir Session::plugin_dir(QString plugin)
{
    QDir plugin_dir(QCoreApplication::applicationDirPath());
    plugin_dir.cdUp();
    plugin_dir.cd("lib");
    plugin_dir.cd("dash");
    plugin_dir.cd("plugins");
    plugin_dir.cd(plugin);

    return plugin_dir;
}

QString Session::fmt_plugin(QString plugin)
{
    plugin.remove(0, 3);
    plugin.replace("_", " ");

    return plugin;
}

Session::Theme::Mode Session::Theme::from_str(QString mode)
{
    // defaults to light mode if unknown
    return (mode == "Dark") ? Session::Theme::Dark : Session::Theme::Light;
}

QString Session::Theme::to_str(Session::Theme::Mode mode)
{
    switch (mode) {
        case Session::Theme::Light:
            return "Light";
            break;
        case Session::Theme::Dark:
            return "Dark";
            break;
        default:
            return QString();
    }
}

Session::Theme::Theme(QSettings &settings)
    : mode(static_cast<Theme::Mode>(settings.value("Theme/mode", Session::Theme::Light).toUInt()))
{
    this->colors_[Session::Theme::Light] = QColor(settings.value("Theme/Color/light", "#000000").toString());
    this->colors_[Session::Theme::Dark] = QColor(settings.value("Theme/Color/dark", "#ffffff").toString());
}

QPalette Session::Theme::palette() const
{
    QPalette palette;
    auto color = this->color();
    palette.setColor(QPalette::Base, color);
    color.setAlphaF(.5);
    palette.setColor(QPalette::AlternateBase, color);

    return palette;
}

Session::Layout::ControlBar::ControlBar(QSettings &settings, Arbiter &arbiter)
    : enabled(settings.value("Layout/ControlBar/enabled", true).toBool())
    , curr_quick_view(nullptr)
{
    this->quick_views_ = {
        new NullQuickView(arbiter),
        new VolumeQuickView(arbiter),
        new BrightnessQuickView(arbiter),
        new ComboQuickView(arbiter)
    };

    this->curr_quick_view = this->quick_views_.value(settings.value("Layout/ControlBar/quick_view", 0).toInt());
}

Session::Layout::Fullscreen::Fullscreen(QSettings &settings, Arbiter &arbiter)
    : enabled(false)
    , curr_toggler(nullptr)
    , on_start(settings.value("Layout/Fullscreen/on_start", false).toBool())
{
    this->togglers_ = {
        new NullFullscreenToggler(arbiter),
        new BarFullscreenToggler(arbiter),
        new ButtonFullscreenToggler(arbiter)
    };

    this->curr_toggler = this->togglers_.value(settings.value("Layout/Fullscreen/toggler", 0).toInt());
}

Session::Layout::Layout(QSettings &settings, Arbiter &arbiter)
    : scale(settings.value("Layout/scale", 1.0).toDouble())
    , status_bar(settings.value("Layout/status_bar", false).toBool())
    , fullscreen(settings, arbiter)
    , control_bar(settings, arbiter)
    , openauto_page(new OpenAutoPage(arbiter))
    , curr_page(nullptr)
{
    this->pages_ = {
        this->openauto_page,
        new MediaPage(arbiter),
        new VehiclePage(arbiter),
        new LauncherPage(arbiter),
        new SettingsPage(arbiter)
    };

    settings.beginGroup("Layout");
    settings.beginGroup("Page");
    for (int i = 0; i < this->pages_.size(); i++) {
        auto page = this->page(i);
        if (page->toggleale()) {
            if (!settings.value(QString::number(i), true).toBool())
                page->enable(false);
        }
    }
    settings.endGroup();
    settings.endGroup();

    for (auto page : this->pages_) {
        if (page->enabled()) {
            this->curr_page = page;
            break;
        }
    }
}

Page *Session::Layout::next_enabled_page(Page *page) const
{
    auto id = this->page_id(page);
    do {
        id = (id + 1) % this->pages_.size();
    } while (!this->page(id)->enabled());

    return this->page(id);
}


const char *Session::System::Brightness::AUTO_PLUGIN = "auto";

Session::System::Brightness::Brightness(QSettings &settings)
    : plugin(settings.value("System/Brightness/plugin", Session::System::Brightness::AUTO_PLUGIN).toString())
    , value(settings.value("System/Brightness/value", 255).toUInt())
    , loader_()
{
    for (const auto &file : Session::plugin_dir("brightness").entryInfoList(QDir::Files)) {
        auto path = file.absoluteFilePath();
        if (QLibrary::isLibrary(path)) {
            auto name = Session::fmt_plugin(file.baseName());
            if (auto plugin = qobject_cast<BrightnessPlugin *>(QPluginLoader(path).instance()))
                this->plugin_infos_.append({name, path, plugin->supported(), plugin->priority()});
        }
    }
    std::sort(this->plugin_infos_.begin(), this->plugin_infos_.end());

    this->load();
}

void Session::System::Brightness::load()
{
    if (this->loader_.isLoaded())
        this->loader_.unload();

    if ((this->plugin == Session::System::Brightness::AUTO_PLUGIN) && !this->plugin_infos_.isEmpty()) {
        this->loader_.setFileName(this->plugin_infos_.first().path);
    }
    else {
        auto it = std::find_if(this->plugin_infos_.begin(), this->plugin_infos_.end(), [this](PluginInfo &info){
            return info.name == this->plugin;
        });
        if (it != this->plugin_infos_.end())
            this->loader_.setFileName(it->path);
    }
}

void Session::System::Brightness::set()
{
    if (auto plugin = qobject_cast<BrightnessPlugin *>(this->loader_.instance()))
        plugin->set(this->value);
}

void Session::System::Brightness::reset()
{
    if (auto plugin = qobject_cast<BrightnessPlugin *>(this->loader_.instance()))
        plugin->set(255);
}

const QList<QString> &Session::System::Brightness::plugins() const
{
    // generates only once
    static const QList<QString> plugins = [this]{
        QList<QString> names;
        for (const auto &info : this->plugin_infos_) {
            if (info.supported)
                names.append(info.name);
        }
        return names;
    }();
    return plugins;
}

Session::System::System(QSettings &settings, Arbiter &arbiter)
    : clock()
    , server(arbiter)
    , bluetooth(arbiter)
    , brightness(settings)
    , volume(settings.value("System/volume", 50).toUInt())
{
    this->set_volume();
}

void Session::System::set_volume() const
{
    QProcess::startDetached("wpctl", {"set-volume", "@DEFAULT_AUDIO_SINK@", QString("%1%").arg(this->volume)});
}

void Session::System::reboot(QSettings &settings)
{
    settings.sync();
    QProcess::startDetached("systemctl", {"reboot"});
}

void Session::System::shutdown(QSettings &settings)
{
    settings.sync();
    QProcess::startDetached("systemctl", {"poweroff"});
}

QFrame *Session::Forge::br(bool vertical)
{
    auto br = new QFrame();
    br->setLineWidth(2);
    br->setFrameShape(vertical ? QFrame::VLine : QFrame::HLine);
    br->setFrameShadow(QFrame::Plain);

    return br;
}

void Session::Forge::to_touch_scroller(QAbstractScrollArea *area)
{
    auto policy = QVariant::fromValue<QScrollerProperties::OvershootPolicy>(QScrollerProperties::OvershootAlwaysOff);
    auto properties = QScroller::scroller(area->viewport())->scrollerProperties();
    properties.setScrollMetric(QScrollerProperties::VerticalOvershootPolicy, policy);
    properties.setScrollMetric(QScrollerProperties::HorizontalOvershootPolicy, policy);

    area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    QScroller::grabGesture(area->viewport(), QScroller::LeftMouseButtonGesture);
    QScroller::scroller(area->viewport())->setScrollerProperties(properties);
}

void Session::Forge::symbolize(QAbstractButton *button)
{
    auto policy = button->sizePolicy();
    policy.setRetainSizeWhenHidden(true);
    button->setSizePolicy(policy);
    button->setFocusPolicy(Qt::NoFocus);
    button->setAttribute(Qt::WA_TransparentForMouseEvents, true);
}

Session::Forge::Forge(Arbiter &arbiter)
    : arbiter_(arbiter)
{
}

void Session::Forge::iconize(QString name, QAbstractButton *button, uint8_t size) const
{
    this->iconize(name, QString(), button, size);
}

void Session::Forge::iconize(QString name, QString alt_name, QAbstractButton *button, uint8_t size) const
{
    QIcon icon(new IconEngine(this->arbiter_, QString(":/icons/%1.svg").arg(name), false));
    if (!alt_name.isNull())
        icon.addFile(QString(":/icons/%1.svg").arg(alt_name), QSize(), QIcon::Normal, QIcon::On);
    this->iconize(icon, button, size);
}

void Session::Forge::iconize(QIcon &icon, QAbstractButton *button, uint8_t size) const
{
    auto scaled = size * this->arbiter_.layout().scale;
    button->setIconSize(QSize(scaled, scaled));
    button->setIcon(icon);
}

QFont Session::Forge::font(int size, bool mono) const
{
    auto name = mono ? "Titillium Web" : "Montserrat";
    auto scaled = size * this->arbiter_.layout().scale;

    return QFont(name, scaled);
}

QWidget *Session::Forge::brightness_slider(bool buttons) const
{
    auto widget = new QWidget();
    auto layout = new QHBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto slider = new QSlider(Qt::Orientation::Horizontal);
    slider->setTracking(false);
    slider->setRange(76, 255);
    slider->setValue(this->arbiter_.system().brightness.value);
    // untracked: a drag lands once on release, a keypad step at once
    QObject::connect(slider, &QSlider::valueChanged, [this](int value){ this->arbiter_.set_brightness(value); });
    QObject::connect(&this->arbiter_, &Arbiter::brightness_changed, slider, [slider](int brightness){
        QSignalBlocker blocker(slider);
        slider->setValue(brightness);
    });

    if (buttons) {
        auto dim_button = new QPushButton();
        dim_button->setFlat(true);
        this->iconize("brightness_low", dim_button, 26);
        QObject::connect(dim_button, &QPushButton::clicked, [this]{ this->arbiter_.decrease_brightness(18); });

        auto brighten_button = new QPushButton();
        brighten_button->setFlat(true);
        this->iconize("brightness_high", brighten_button, 26);
        QObject::connect(brighten_button, &QPushButton::clicked, [this]{ this->arbiter_.increase_brightness(18); });

        layout->addWidget(dim_button);
        layout->addWidget(brighten_button);
    }

    layout->insertWidget(buttons ? 1 : 0, slider, 4);

    return widget;
}

QWidget *Session::Forge::volume_slider(bool buttons) const
{
    auto widget = new QWidget();
    auto layout = new QHBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto slider = new QSlider(Qt::Orientation::Horizontal);
    slider->setTracking(false);
    slider->setRange(0, 100);
    slider->setValue(this->arbiter_.system().volume);
    QObject::connect(slider, &QSlider::valueChanged, [this](int value){ this->arbiter_.set_volume(value); });
    QObject::connect(&this->arbiter_, &Arbiter::volume_changed, slider, [slider](int volume){
        QSignalBlocker blocker(slider);
        slider->setValue(volume);
    });

    if (buttons) {
        auto lower_button = new QPushButton();
        lower_button->setFlat(true);
        this->iconize("volume_down", lower_button, 26);
        QObject::connect(lower_button, &QPushButton::clicked, [this]{ this->arbiter_.decrease_volume(10); });

        auto raise_button = new QPushButton();
        raise_button->setFlat(true);
        this->iconize("volume_up", raise_button, 26);
        QObject::connect(raise_button, &QPushButton::clicked, [this]{ this->arbiter_.increase_volume(10); });

        layout->addWidget(lower_button);
        layout->addWidget(raise_button);
    }

    layout->insertWidget(buttons ? 1 : 0, slider, 4);

    return widget;
}

Session::AndroidAuto::AndroidAuto(Arbiter &arbiter)
    : handler(new AAHandler())
{

}

const QString Session::Core::BINDINGS_GROUP("Core/Bindings");

const QStringList Session::Core::LEGACY_ACTION_IDS = {
    "aa_left", "aa_enter", "aa_right", "aa_up", "aa_down", "aa_back", "aa_home", "aa_phone", "aa_call_end",
    "aa_play", "aa_pause", "aa_prev_track", "aa_next_track", "aa_toggle_play", "aa_voice", "aa_scroll_up",
    "aa_scroll_down", "toggle_dark_mode", "decrease_brightness", "increase_brightness", "decrease_volume",
    "increase_volume", "toggle_fullscreen", "show_page_android_auto", "show_page_media", "show_page_vehicle",
    "show_page_camera", "show_page_launcher", "show_page_settings", "cycle_page"
};

// Renault 1657 keypad, as bridged by kbd-bridge
const QList<QPair<QString, QString>> Session::Core::DEFAULT_BINDINGS = {
    {"aa_left", "Left"},
    {"aa_right", "Right"},
    {"aa_up", "Up"},
    {"aa_down", "Down"},
    {"aa_enter", "Return"},
    {"aa_back", "Esc"},
    {"aa_scroll_up", "PgUp"},
    {"aa_scroll_down", "PgDown"},
    {"aa_phone", "F1"},
    {"aa_home", "F2"},
    {"aa_voice", "F3"},
    {"aa_toggle_play", "F4"},
    {"cycle_page", "F5"},
    {"toggle_dark_mode", "F6"},
    {"toggle_fullscreen", "F7"},
    {"show_page_settings", "F8"},
};

QMap<QString, QString> Session::Core::load_bindings(QSettings &settings, const QStringList &ids)
{
    settings.beginGroup("Core/Action");
    QList<QPair<QString, QString>> migrated;
    const auto legacy = settings.childKeys();
    for (const auto &index : legacy) {
        bool ok = false;
        auto id = Session::Core::LEGACY_ACTION_IDS.value(index.toInt(&ok));
        auto key = settings.value(index).toString();
        if (ok && ids.contains(id))
            migrated.append({id, key});
        else
            DASH_LOG(warning) << "[Core] dropping binding " << key.toStdString() << " of retired action " << index.toStdString();
    }
    settings.endGroup();

    if (!legacy.isEmpty()) {
        settings.remove("Core/Action");
        for (const auto &[id, key] : migrated) {
            auto path = Session::Core::BINDINGS_GROUP + "/" + id;
            if (!settings.contains(path))
                settings.setValue(path, key);
        }
        DASH_LOG(info) << "[Core] migrated " << migrated.size() << " index-based action bindings";
    }

    QMap<QString, QString> bindings;
    QSet<QString> stored;
    settings.beginGroup(Session::Core::BINDINGS_GROUP);
    for (const auto &id : settings.childKeys()) {
        if (!ids.contains(id))
            continue;
        stored.insert(id);
        auto key = settings.value(id).toString();
        if (!key.isEmpty())
            bindings[id] = key;
    }
    settings.endGroup();

    QSet<QString> taken(bindings.begin(), bindings.end());
    for (const auto &[id, key] : Session::Core::DEFAULT_BINDINGS) {
        if (ids.contains(id) && !stored.contains(id) && !taken.contains(key)) {
            bindings[id] = key;
            taken.insert(key);
        }
    }

    return bindings;
}

Session::Core::Core(QSettings &settings, Arbiter &arbiter)
    : cursor(settings.value("Core/cursor", true).toBool())
{
    using aasdk::proto::enums::ButtonCode;

    this->stylesheets_[Session::Theme::Light] = this->parse_stylesheet(":/stylesheets/light.qss");
    this->stylesheets_[Session::Theme::Dark] = this->parse_stylesheet(":/stylesheets/dark.qss");

    auto window = arbiter.window();
    AAHandler *aa_handler = arbiter.android_auto().handler;
    std::function<bool()> aa_in_front = [&arbiter]{ return arbiter.android_auto_in_front(); };
    auto on_press = [](std::function<void()> func){
        return [func](Action::ActionState state){
            if (state != Action::ActionState::Deactivated)
                func();
        };
    };
    auto later = [&arbiter](std::function<void()> func){
        QMetaObject::invokeMethod(&arbiter, func, Qt::QueuedConnection);
    };

    // navigation keys only drive Android Auto while it is in front; otherwise they navigate dash
    auto aa_button = [this, window, aa_handler](QString id, QString name, ButtonCode::Enum code, std::function<bool()> available){
        this->actions_.append(new Action(id, name, [aa_handler, code](Action::ActionState state){
            aa_handler->injectButtonPressHelper(code, state);
        }, window, available));
    };
    auto aa_wheel = [this, window, aa_handler, on_press](QString id, QString name, openauto::projection::WheelDirection direction, std::function<bool()> available){
        this->actions_.append(new Action(id, name, on_press([aa_handler, direction]{
            aa_handler->injectButtonPress(ButtonCode::SCROLL_WHEEL, direction);
        }), window, available));
    };

    aa_button("aa_left", "Android Auto Left", ButtonCode::LEFT, aa_in_front);
    aa_button("aa_enter", "Android Auto Enter", ButtonCode::ENTER, aa_in_front);
    aa_button("aa_right", "Android Auto Right", ButtonCode::RIGHT, aa_in_front);
    aa_button("aa_up", "Android Auto Up", ButtonCode::UP, aa_in_front);
    aa_button("aa_down", "Android Auto Down", ButtonCode::DOWN, aa_in_front);
    aa_button("aa_back", "Android Auto Back", ButtonCode::BACK, aa_in_front);

    // brings Android Auto to the front first, then acts as its home button
    auto home_injected = std::make_shared<bool>(false);
    this->actions_.append(new Action("aa_home", "Android Auto Home", [&arbiter, aa_handler, later, home_injected](Action::ActionState state){
        if (state != Action::ActionState::Deactivated) {
            *home_injected = arbiter.android_auto_in_front();
            if (!*home_injected) {
                later([&arbiter]{ arbiter.set_curr_page(arbiter.layout().openauto_page); });
                return;
            }
        }
        else if (!*home_injected) {
            return;
        }
        aa_handler->injectButtonPressHelper(ButtonCode::HOME, state);
    }, window));

    aa_button("aa_phone", "Android Auto Phone", ButtonCode::PHONE, {});
    aa_button("aa_call_end", "Android Auto Call End", ButtonCode::CALL_END, {});
    aa_button("aa_play", "Android Auto Play", ButtonCode::PLAY, {});
    aa_button("aa_pause", "Android Auto Pause", ButtonCode::PAUSE, {});
    aa_button("aa_prev_track", "Android Auto Prev Track", ButtonCode::PREV, {});
    aa_button("aa_next_track", "Android Auto Next Track", ButtonCode::NEXT, {});
    aa_button("aa_toggle_play", "Android Auto Toggle Play", ButtonCode::TOGGLE_PLAY, {});
    aa_button("aa_voice", "Android Auto Voice", ButtonCode::MICROPHONE_1, {});
    aa_wheel("aa_scroll_up", "Android Auto Scroll Up", openauto::projection::WheelDirection::RIGHT, aa_in_front);
    aa_wheel("aa_scroll_down", "Android Auto Scroll Down", openauto::projection::WheelDirection::LEFT, aa_in_front);

    this->actions_.append({
        new Action("toggle_dark_mode", "Toggle Dark Mode", on_press([&arbiter]{ arbiter.toggle_mode(); }), window),
        new Action("decrease_brightness", "Decrease Brightness", on_press([&arbiter]{ arbiter.decrease_brightness(4); }), window),
        new Action("increase_brightness", "Increase Brightness", on_press([&arbiter]{ arbiter.increase_brightness(4); }), window),
        new Action("decrease_volume", "Decrease Volume", on_press([&arbiter]{ arbiter.decrease_volume(2); }), window),
        new Action("increase_volume", "Increase Volume", on_press([&arbiter]{ arbiter.increase_volume(2); }), window),
        new Action("toggle_fullscreen", "Toggle Fullscreen", on_press([&arbiter]{ arbiter.toggle_fullscreen(); }), window)
    });

    for (auto page : arbiter.layout().pages()) {
        auto show = on_press([later, &arbiter, page]{ later([&arbiter, page]{ arbiter.set_curr_page(page); }); });
        this->actions_.append(new Action("show_page_" + page->id(), QString("Show %1 Page").arg(page->name()), show, window));
    }

    this->actions_.append(new Action("cycle_page", "Cycle Page", on_press([later, &arbiter]{
        later([&arbiter]{ arbiter.set_curr_page(arbiter.layout().next_enabled_page(arbiter.layout().curr_page)); });
    }), window));

    QStringList ids;
    for (auto action : this->actions_)
        ids.append(action->id());
    const auto bindings = Session::Core::load_bindings(settings, ids);
    for (auto it = bindings.begin(); it != bindings.end(); ++it)
        this->action(it.key())->set(it.value());

    QFontDatabase::addApplicationFont(":/fonts/Titillium_Web/TitilliumWeb-Regular.ttf");
    QFontDatabase::addApplicationFont(":/fonts/Montserrat/Montserrat-LightItalic.ttf");
    QFontDatabase::addApplicationFont(":/fonts/Montserrat/Montserrat-Regular.ttf");

    if (qApp)
        qApp->setFont(arbiter.forge().font(14));

    this->set_cursor();
}

Action *Session::Core::action(const QString &id) const
{
    for (auto action : this->actions_) {
        if (action->id() == id)
            return action;
    }
    return nullptr;
}

QString Session::Core::stylesheet(Theme::Mode mode, float scale) const
{
    return scale_stylesheet(this->stylesheet(mode), scale);
}

void Session::Core::set_cursor() const
{
    if (qApp)
        qApp->setOverrideCursor(this->cursor ? Qt::ArrowCursor : Qt::BlankCursor);
}

QString Session::Core::parse_stylesheet(QString path) const
{
    QFile file(path);
    file.open(QFile::ReadOnly | QFile::Text);
    QTextStream stream(&file);

    return stream.readAll();
}

Session::Session(Arbiter &arbiter)
    : settings_()
    , theme_(settings_)
    , layout_(settings_, arbiter)
    , system_(settings_, arbiter)
    , forge_(arbiter)
    , android_auto_(arbiter)
    , core_(settings_, arbiter)
{
}

void Session::update()
{
    if (qApp) {
        qApp->setPalette(this->theme_.palette());
        qApp->setStyleSheet(this->core_.stylesheet(this->theme_.mode, this->layout_.scale));
    }
}
