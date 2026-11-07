#include <QCanBus>

#include "DashLog.hpp"
#include "app/config.hpp"

#include "canbus/socketcanbus.hpp"

SocketCANBus::SocketCANBus(QString interface, QString plugin, QObject *parent)
{
    this->setParent(parent);

    QString error;
    this->bus.reset(QCanBus::instance()->createDevice(plugin, interface, &error));
    if (!this->bus) {
        DASH_LOG(error) << "[SocketCANBus] cannot create " << plugin.toStdString() << " device "
                        << interface.toStdString() << ": " << error.toStdString();
        return;
    }

    connect(this->bus.get(), &QCanBusDevice::framesReceived, this, &SocketCANBus::dispatch_frames);
    connect(this->bus.get(), &QCanBusDevice::errorOccurred, this, [this](QCanBusDevice::CanBusError) {
        DASH_LOG(error) << "[SocketCANBus] " << this->bus->errorString().toStdString();
    });

    DASH_LOG(info) << "[SocketCANBus] connecting " << plugin.toStdString() << " interface " << interface.toStdString();
    if (!this->bus->connectDevice())
        DASH_LOG(error) << "[SocketCANBus] cannot connect " << interface.toStdString() << ": "
                        << this->bus->errorString().toStdString();
}

SocketCANBus::~SocketCANBus()
{
    if (this->bus && this->bus->state() == QCanBusDevice::ConnectedState)
        this->bus->disconnectDevice();
}

SocketCANBus *SocketCANBus::get_instance()
{
    static SocketCANBus bus(Config::get_instance()->get_vehicle_interface());
    return &bus;
}

bool SocketCANBus::is_connected() const
{
    return this->bus && this->bus->state() == QCanBusDevice::ConnectedState;
}

bool SocketCANBus::writeFrame(QCanBusFrame frame)
{
    return this->is_connected() && this->bus->writeFrame(frame);
}

void SocketCANBus::dispatch_frames()
{
    for (const auto &frame : this->bus->readAllFrames()) {
        if (!frame.isValid() || frame.frameType() != QCanBusFrame::DataFrame)
            continue;
        auto handlers = this->callbacks.find(frame.frameId());
        if (handlers == this->callbacks.end())
            continue;
        for (const auto &callback : handlers->second)
            callback(frame.payload());
    }
}

void SocketCANBus::registerFrameHandler(int id, std::function<void(QByteArray)> callback)
{
    auto frame_id = static_cast<quint32>(id);
    this->callbacks[frame_id].push_back(std::move(callback));

    bool extended = frame_id > 0x7FF;
    QCanBusDevice::Filter filter;
    filter.frameId = frame_id;
    filter.frameIdMask = extended ? 0x1FFFFFFFu : 0x7FFu;
    filter.format = extended ? QCanBusDevice::Filter::MatchExtendedFormat : QCanBusDevice::Filter::MatchBaseFormat;
    filter.type = QCanBusFrame::DataFrame;
    this->filters_.append(filter);

    if (this->bus)
        this->bus->setConfigurationParameter(QCanBusDevice::RawFilterKey, QVariant::fromValue(this->filters_));
}
