#pragma once

#include <QObject>
#include <QPointer>
#include "plugins/vehicle_plugin.hpp"
#include "app/widgets/climate.hpp"
#include "app/widgets/vehicle.hpp"
#include "canbus/socketcanbus.hpp"

class Test : public QObject, VehiclePlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID VehiclePlugin_iid FILE "test.json")
    Q_INTERFACES(VehiclePlugin)

   public:
    Test() {};
    ~Test();
    QList<QWidget *> widgets() override;
    bool init(ICANBus*) override;

   private:
    // the vehicle tab is owned by the page it is added to
    QPointer<Climate> climate;
    QPointer<Vehicle> vehicle;
};
