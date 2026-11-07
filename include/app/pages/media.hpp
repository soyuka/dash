#pragma once

#include <QFileInfo>
#include <QMap>
#include <QPluginLoader>
#include <QString>
#include <QtWidgets>

#include "app/config.hpp"
#include "app/pages/page.hpp"
#include "app/widgets/selector.hpp"
#include "app/widgets/tuner.hpp"

class Arbiter;

class MediaPage : public QTabWidget, public Page {
    Q_OBJECT

   public:
    MediaPage(Arbiter &arbiter, QWidget *parent = nullptr);

    void init() override;
};

class BluetoothPlayerTab : public QWidget {
    Q_OBJECT

   public:
    BluetoothPlayerTab(Arbiter &arbiter, QWidget *parent = nullptr);

   private:
    Arbiter &arbiter;

    QWidget *track_widget();
    QWidget *controls_widget();
};

class RadioPlayerTab : public QWidget {
    Q_OBJECT

   public:
    RadioPlayerTab(Arbiter &arbiter, QWidget *parent = nullptr);
    ~RadioPlayerTab();

   private:
    static QMap<QString, QFileInfo> get_plugins();

    Arbiter &arbiter;
    Config *config;
    QMap<QString, QFileInfo> plugins;
    QPluginLoader loader;
    Tuner *tuner;
    Selector *plugin_selector;
    QPushButton *play_button;

    void load_plugin();
    QWidget *dialog_body();
    QWidget *tuner_widget();
    QWidget *controls_widget();

};
