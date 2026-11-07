#include <BluezQt/PendingCall>

#include "app/window.hpp"
#include "app/pages/media.hpp"
#include "plugins/radio_plugin.hpp"

MediaPage::MediaPage(Arbiter &arbiter, QWidget *parent)
    : QTabWidget(parent)
    , Page(arbiter, "Media", "play_circle_outline", true, this)
{
    
}

void MediaPage::init()
{
    this->addTab(new RadioPlayerTab(this->arbiter, this), "Radio");
    this->addTab(new BluetoothPlayerTab(this->arbiter, this), "Bluetooth");
}

BluetoothPlayerTab::BluetoothPlayerTab(Arbiter &arbiter, QWidget *parent)
    : QWidget(parent)
    , arbiter(arbiter)
{
    QVBoxLayout *layout = new QVBoxLayout(this);

    layout->addWidget(this->track_widget());
    layout->addWidget(this->controls_widget());
}

QWidget *BluetoothPlayerTab::track_widget()
{
    BluezQt::MediaPlayerPtr media_player = this->arbiter.system().bluetooth.get_media_player().second;
    AAHandler *aa_handler = this->arbiter.android_auto().handler;

    QWidget *widget = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(widget);

    QLabel *artist_hdr = new QLabel("Artist", widget);
    QLabel *artist = new QLabel((media_player != nullptr) ? media_player->track().artist() : QString(), widget);
    artist->setIndent(16);
    layout->addWidget(artist_hdr);
    layout->addWidget(artist);

    QLabel *album_hdr = new QLabel("Album", widget);
    QLabel *album = new QLabel((media_player != nullptr) ? media_player->track().album() : QString(), widget);
    album->setIndent(16);
    layout->addWidget(album_hdr);
    layout->addWidget(album);

    QLabel *title_hdr = new QLabel("Title", widget);
    QLabel *title = new QLabel((media_player != nullptr) ? media_player->track().title() : QString(), widget);
    title->setIndent(16);
    layout->addWidget(title_hdr);
    layout->addWidget(title);

    QLabel *albumArt = new QLabel(widget);
    layout->addWidget(albumArt);

    connect(&this->arbiter.system().bluetooth, &Bluetooth::media_player_track_changed, [artist, album, title](BluezQt::MediaPlayerTrack track){
        artist->setText(track.artist());
        album->setText(track.album());
        title->setText(track.title());
    });
    connect(aa_handler, &AAHandler::aa_media_metadata_update, [artist, album, title, albumArt](const aasdk::proto::messages::MediaInfoChannelMetadataData& metadata){
        title->setText(QString::fromStdString(metadata.track_name()));
        if(metadata.has_artist_name()) artist->setText(QString::fromStdString(metadata.artist_name()));
        if(metadata.has_album_name()) album->setText(QString::fromStdString(metadata.album_name()));
        if(metadata.has_album_art()){
            QImage art;
            art.loadFromData(QByteArray::fromStdString(metadata.album_art()));
            albumArt->setPixmap(QPixmap::fromImage(art));
        }
    
    });


    return widget;
}

QWidget *BluetoothPlayerTab::controls_widget()
{
    BluezQt::MediaPlayerPtr media_player = this->arbiter.system().bluetooth.get_media_player().second;

    QWidget *widget = new QWidget(this);
    QHBoxLayout *layout = new QHBoxLayout(widget);

    QPushButton *previous_button = new QPushButton(widget);
    previous_button->setFlat(true);
    this->arbiter.forge().iconize("skip_previous", previous_button, 56);
    connect(previous_button, &QPushButton::clicked, [this]{
        BluezQt::MediaPlayerPtr media_player = this->arbiter.system().bluetooth.get_media_player().second;
        if (media_player != nullptr)
            media_player->previous()->waitForFinished();
    });
    layout->addWidget(previous_button);

    QPushButton *play_button = new QPushButton(widget);
    play_button->setFlat(true);
    play_button->setCheckable(true);
    bool status = (media_player != nullptr) ? media_player->status() == BluezQt::MediaPlayer::Status::Playing : false;
    play_button->setChecked(status);
    this->arbiter.forge().iconize("play", "pause", play_button, 56);
    connect(play_button, &QPushButton::clicked, [this, play_button](bool checked = false){
        play_button->setChecked(!checked);

        BluezQt::MediaPlayerPtr media_player = this->arbiter.system().bluetooth.get_media_player().second;
        if (media_player != nullptr) {
            if (checked)
                media_player->play()->waitForFinished();
            else
                media_player->pause()->waitForFinished();
        }
    });
    connect(&this->arbiter.system().bluetooth, &Bluetooth::media_player_status_changed, [play_button](BluezQt::MediaPlayer::Status status){
        play_button->setChecked(status == BluezQt::MediaPlayer::Status::Playing);
    });
    layout->addWidget(play_button);

    QPushButton *forward_button = new QPushButton(widget);
    forward_button->setFlat(true);
    this->arbiter.forge().iconize("skip_next", forward_button, 56);
    connect(forward_button, &QPushButton::clicked, [this]{
        BluezQt::MediaPlayerPtr media_player = this->arbiter.system().bluetooth.get_media_player().second;
        if (media_player != nullptr)
            media_player->next()->waitForFinished();
    });
    layout->addWidget(forward_button);

    return widget;
}

QMap<QString, QFileInfo> RadioPlayerTab::get_plugins()
{
    QMap<QString, QFileInfo> plugins;
    for (auto plugin : Session::plugin_dir("radio").entryInfoList(QDir::Files)) {
        if (QLibrary::isLibrary(plugin.absoluteFilePath()))
            plugins[Session::fmt_plugin(plugin.baseName())] = plugin;
    }

    return plugins;
}

RadioPlayerTab::RadioPlayerTab(Arbiter &arbiter, QWidget *parent)
    : QWidget(parent)
    , arbiter(arbiter)
    , config(Config::get_instance())
    , plugins(RadioPlayerTab::get_plugins())
    , loader()
    , tuner(new Tuner(this->arbiter))
    , plugin_selector(new Selector(this->plugins.keys(), this->config->get_radio_plugin(), this->arbiter.forge().font(14), this->arbiter, nullptr, "unloader"))
    , play_button(new QPushButton())
{
    this->play_button->setFlat(true);
    this->play_button->setCheckable(true);
    this->arbiter.forge().iconize("play", "stop", this->play_button, 48);
    connect(this->play_button, &QPushButton::clicked, [this](bool checked){
        if (RadioPlugin *plugin = qobject_cast<RadioPlugin *>(this->loader.instance())) {
            if (checked)
                plugin->play();
            else
                plugin->stop();
        }
        else {
            this->play_button->setChecked(false);
        }
    });

    this->tuner->setValue(this->config->get_radio_station());

    auto layout = new QVBoxLayout(this);
    layout->addStretch(1);
    layout->addWidget(this->tuner_widget(), 1);
    layout->addWidget(this->controls_widget(), 3);
    layout->addStretch(1);

    this->load_plugin();
}

RadioPlayerTab::~RadioPlayerTab()
{
    this->loader.unload();
}

void RadioPlayerTab::load_plugin()
{
    if (this->loader.isLoaded())
        this->loader.unload();

    this->play_button->setChecked(false);

    auto key = this->plugin_selector->get_current();
    if (!key.isNull()) {
        this->loader.setFileName(this->plugins[key].absoluteFilePath());
        if (RadioPlugin *plugin = qobject_cast<RadioPlugin *>(this->loader.instance())) {
            plugin->freq(this->tuner->value() * 100000);
        }
    }
    this->config->set_radio_plugin(key);
}

QWidget *RadioPlayerTab::dialog_body()
{
    auto widget = new QWidget(this);
    auto layout = new QVBoxLayout(widget);

    layout->addStretch();
    layout->addWidget(this->plugin_selector, 0, Qt::AlignCenter);
    layout->addStretch();

    return widget;
}

QWidget *RadioPlayerTab::tuner_widget()
{
    auto widget = new QWidget(this);
    auto layout = new QHBoxLayout(widget);
    layout->setSpacing(0);

    auto dialog = new Dialog(this->arbiter, true, this->window());
    dialog->set_body(this->dialog_body());

    auto load_button = new QPushButton("load");
    connect(load_button, &QPushButton::clicked, [this]{ this->load_plugin(); });
    dialog->set_button(load_button);

    auto settings_button = new QPushButton();
    settings_button->setFlat(true);
    this->arbiter.forge().iconize("settings", settings_button, 24);
    connect(settings_button, &QPushButton::clicked, [dialog]{ dialog->open(); });

    auto station = new QLabel(QString::number(this->tuner->sliderPosition() / 10.0, 'f', 1));
    station->setFont(this->arbiter.forge().font(36, true));
    connect(this->tuner, &Tuner::valueChanged, [this, station](int freq){
        this->config->set_radio_station(freq);
        station->setText(QString::number(freq / 10.0, 'f', 1));
        if (RadioPlugin *plugin = qobject_cast<RadioPlugin *>(this->loader.instance()))
            plugin->freq(freq * 100000);
    });

    // auto info = new QLabel("station info");
    // info->setWordWrap(true);

    layout->addStretch(2);
    layout->addWidget(settings_button);
    layout->addWidget(station, 2);
    // layout->addWidget(info, 3);
    layout->addWidget(this->play_button, 3);
    layout->addStretch(2);

    return widget;
}

QWidget *RadioPlayerTab::controls_widget()
{
    auto widget = new QWidget();
    auto layout = new QHBoxLayout(widget);

    auto prev_station = new QPushButton();
    prev_station->setFlat(true);
    this->arbiter.forge().iconize("chevron_left", prev_station, 56);
    connect(prev_station, &QPushButton::clicked, [tuner = this->tuner]{
        tuner->setSliderPosition(tuner->sliderPosition() - 1);
    });

    auto next_station = new QPushButton();
    next_station->setFlat(true);
    this->arbiter.forge().iconize("chevron_right", next_station, 56);
    connect(next_station, &QPushButton::clicked, [tuner = this->tuner]{
        tuner->setSliderPosition(tuner->sliderPosition() + 1);
    });

    layout->addStretch(1);
    layout->addWidget(prev_station);
    layout->addWidget(this->tuner, 4);
    layout->addWidget(next_station);
    layout->addStretch(1);

    return widget;
}
