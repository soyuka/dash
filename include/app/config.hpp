#pragma once

#include "openauto/Configuration/Configuration.hpp"
#include "canbus/ICANBus.hpp"

#include <QObject>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QWidget>

/*
    NOTE: to only be used for page-specific config

    this is temporary, will soon be migrated to new cool ways
*/

class Config : public QObject {
    Q_OBJECT

   public:
    std::shared_ptr<openauto::configuration::Configuration> openauto_config;
    openauto::configuration::Configuration::ButtonCodes openauto_button_codes;

    Config();

    inline double get_radio_station() { return this->radio_station; }
    inline void set_radio_station(double radio_station)
    {
        this->radio_station = radio_station;
        this->settings.setValue("Pages/Media/Radio/station", this->radio_station);
    }

    inline bool get_radio_muted() { return this->radio_muted; }
    inline void set_radio_muted(bool radio_muted)
    {
        this->radio_muted = radio_muted;
        this->settings.setValue("Pages/Media/Radio/muted", this->radio_muted);
    }

    inline QString get_radio_plugin() { return this->radio_plugin; }
    inline void set_radio_plugin(QString radio_plugin)
    {
        this->radio_plugin = radio_plugin;
        if (this->radio_plugin == "unloader")
            this->settings.remove("Pages/Media/Radio/plugin");
        else
            this->settings.setValue("Pages/Media/Radio/plugin", this->radio_plugin);
    }

    inline bool get_si_units() { return this->si_units; }
    inline void set_si_units(bool si_units)
    {
        this->si_units = si_units;
        this->settings.setValue("Pages/Vehicle/si_units", this->si_units);
        emit si_units_changed(this->si_units);
    }

    inline ICANBus::VehicleBusType get_vehicle_can_bus() { return this->vehicle_can_bus; }
    inline void set_vehicle_can_bus(ICANBus::VehicleBusType vehicle_can_bus)
    {
        this->vehicle_can_bus = vehicle_can_bus;
        this->settings.setValue("Pages/Vehicle/can_bus", this->vehicle_can_bus);
        emit vehicle_can_bus_changed(this->vehicle_can_bus);
    }

    inline QString get_vehicle_interface() { return this->vehicle_interface; }
    inline void set_vehicle_interface(QString vehicle_interface)
    {
        this->vehicle_interface = vehicle_interface;
        if (this->vehicle_interface == "disabled")
            this->settings.remove("Pages/Vehicle/interface");
        else
            this->settings.setValue("Pages/Vehicle/interface", this->vehicle_interface);
        emit vehicle_interface_changed(this->vehicle_interface);
    }

    inline QString get_vehicle_plugin() { return this->vehicle_plugin; }
    inline void set_vehicle_plugin(QString vehicle_plugin)
    {
        this->vehicle_plugin = vehicle_plugin;
        if (this->vehicle_plugin == "unloader")
            this->settings.remove("Pages/Vehicle/plugin");
        else
            this->settings.setValue("Pages/Vehicle/plugin", this->vehicle_plugin);
    }

    inline bool get_show_aa_connected() { return this->show_aa_connected; }
    inline void set_show_aa_connected(bool enabled)
    {
        this->show_aa_connected = enabled;
        this->settings.setValue("Pages/OpenAuto/show_aa_connected", this->show_aa_connected);
    }

    inline const QStringList &get_launcher_plugins() { return this->launcher_plugins; }
    inline void set_launcher_plugin(QString plugin, bool remove = false)
    {
        if (remove)
            this->launcher_plugins.removeOne(plugin);
        else
            this->launcher_plugins.append(plugin);

        // need to sync idxs
        this->settings.remove("Pages/Launcher");
        for (int i = 0; i < this->launcher_plugins.size(); i++)
            this->settings.setValue(QString("Pages/Launcher/%1").arg(i), this->launcher_plugins[i]);
    }

    static Config *get_instance();

   private:
    QSettings settings;
    double radio_station;
    bool radio_muted;
    QString radio_plugin;
    bool si_units;
    ICANBus::VehicleBusType vehicle_can_bus;
    QString vehicle_interface;
    QString vehicle_plugin;
    bool show_aa_connected;
    QStringList launcher_plugins;

   signals:
    void si_units_changed(bool si_units);
    void vehicle_can_bus_changed(ICANBus::VehicleBusType state);
    void vehicle_interface_changed(QString interface);
};
