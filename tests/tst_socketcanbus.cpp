#include <QCanBus>
#include <QCanBusDevice>
#include <QtTest>

#include "canbus/socketcanbus.hpp"

namespace {
using Received = QList<QPair<quint32, QByteArray>>;

std::unique_ptr<QCanBusDevice> virtual_sender()
{
    std::unique_ptr<QCanBusDevice> device(QCanBus::instance()->createDevice("virtualcan", "can0"));
    if (device)
        device->connectDevice();
    return device;
}
}

class SocketCANBusTest : public QObject {
    Q_OBJECT

   private Q_SLOTS:
    void dispatches_frames_to_handlers_by_id()
    {
        SocketCANBus bus("can0", "virtualcan");
        QTRY_VERIFY(bus.is_connected());

        Received received;
        bus.registerFrameHandler(0x123, [&](QByteArray payload){ received.append({0x123, payload}); });
        bus.registerFrameHandler(0x18DAF110, [&](QByteArray payload){ received.append({0x18DAF110, payload}); });

        auto tx = virtual_sender();
        QVERIFY(tx);
        QTRY_COMPARE(tx->state(), QCanBusDevice::ConnectedState);

        QCanBusFrame extended(0x18DAF110, QByteArray::fromHex("0203"));
        extended.setExtendedFrameFormat(true);
        tx->writeFrame(QCanBusFrame(0x123, QByteArray::fromHex("01")));
        tx->writeFrame(QCanBusFrame(0x321, QByteArray::fromHex("ff")));
        tx->writeFrame(extended);

        QTRY_COMPARE(received, Received({{0x123, QByteArray::fromHex("01")}, {0x18DAF110, QByteArray::fromHex("0203")}}));
    }

    void calls_every_handler_registered_for_an_id()
    {
        SocketCANBus bus("can0", "virtualcan");
        int first = 0;
        int second = 0;
        bus.registerFrameHandler(0x2A, [&](QByteArray){ first++; });
        bus.registerFrameHandler(0x2A, [&](QByteArray){ second++; });

        auto tx = virtual_sender();
        QTRY_COMPARE(tx->state(), QCanBusDevice::ConnectedState);
        tx->writeFrame(QCanBusFrame(0x2A, QByteArray::fromHex("00")));

        QTRY_COMPARE(first, 1);
        QCOMPARE(second, 1);
    }

    void installs_kernel_filters_matching_the_id_format()
    {
        SocketCANBus bus("can0", "virtualcan");
        bus.registerFrameHandler(0x7DF, [](QByteArray){});
        bus.registerFrameHandler(0x18DAF110, [](QByteArray){});

        auto filters = bus.filters();
        QCOMPARE(filters.size(), 2);
        QCOMPARE(filters[0].frameId, 0x7DFu);
        QCOMPARE(filters[0].frameIdMask, 0x7FFu);
        QCOMPARE(filters[0].format, QCanBusDevice::Filter::MatchBaseFormat);
        QCOMPARE(filters[1].frameId, 0x18DAF110u);
        QCOMPARE(filters[1].frameIdMask, 0x1FFFFFFFu);
        QCOMPARE(filters[1].format, QCanBusDevice::Filter::MatchExtendedFormat);
    }

    void unusable_device_is_harmless()
    {
        SocketCANBus bus("can0", "no-such-plugin");
        QVERIFY(!bus.is_connected());
        bus.registerFrameHandler(0x123, [](QByteArray){});
        QVERIFY(!bus.writeFrame(QCanBusFrame(0x123, QByteArray::fromHex("01"))));
    }
};

QTEST_GUILESS_MAIN(SocketCANBusTest)
#include "tst_socketcanbus.moc"
