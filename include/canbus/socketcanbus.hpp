#pragma once

#include <functional>
#include <map>
#include <memory>
#include <vector>

#include <QCanBusDevice>
#include <QList>
#include <QString>

#include "canbus/ICANBus.hpp"

class SocketCANBus : public ICANBus {
    Q_OBJECT

   public:
    explicit SocketCANBus(QString interface = "can0", QString plugin = "socketcan", QObject *parent = nullptr);
    ~SocketCANBus() override;

    static SocketCANBus *get_instance();

    void registerFrameHandler(int id, std::function<void(QByteArray)> callback) override;
    bool writeFrame(QCanBusFrame frame) override;

    bool is_connected() const;
    const QList<QCanBusDevice::Filter> &filters() const { return this->filters_; }

   private:
    void dispatch_frames();

    std::unique_ptr<QCanBusDevice> bus;
    std::map<quint32, std::vector<std::function<void(QByteArray)>>> callbacks;
    QList<QCanBusDevice::Filter> filters_;
};
