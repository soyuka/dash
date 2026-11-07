#include <QtTest>

#include "obd/decoders.hpp"
#include "obd/message.hpp"

class ObdTest : public QObject {
    Q_OBJECT

   private Q_SLOTS:
    void parses_mode_pid_and_payload()
    {
        Response resp(QByteArray::fromHex("034105c800000000"));
        QCOMPARE(resp.length, 3);
        QCOMPARE(resp.mode, 0x01);
        QCOMPARE(resp.PID, 0x05);
        QCOMPARE(resp.data.left(1), QByteArray::fromHex("c8"));
    }

    void decodes_bytes_above_127_as_unsigned()
    {
        QCOMPARE(temp(Response(QByteArray::fromHex("034105c8"))), 160.0);
        QCOMPARE(speed(Response(QByteArray::fromHex("03410dc8"))), 200.0);
        QCOMPARE(percentage(Response(QByteArray::fromHex("034104ff"))), 100.0);
        QCOMPARE(rpm(Response(QByteArray::fromHex("04410c1af8"))), 1726.0);
        QCOMPARE(flow(Response(QByteArray::fromHex("044110ff80"))), 654.08);
    }

    void decodes_low_values()
    {
        QCOMPARE(temp(Response(QByteArray::fromHex("03410500"))), -40.0);
        QCOMPARE(rpm(Response(QByteArray::fromHex("04410c0000"))), 0.0);
    }
};

QTEST_GUILESS_MAIN(ObdTest)
#include "tst_obd.moc"
