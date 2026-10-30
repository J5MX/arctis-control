// Unit tests for the low-battery alert logic.
#include <QTest>

#include "batteryalerts.h"

class TestBatteryAlerts : public QObject
{
    Q_OBJECT

private slots:
    void warnsOnceAt20And10()
    {
        BatteryAlerts alerts;
        QCOMPARE(alerts.update(50, false), 0);
        QCOMPARE(alerts.update(20, false), 20);
        QCOMPARE(alerts.update(19, false), 0); // only once
        QCOMPARE(alerts.update(12, false), 0);
        QCOMPARE(alerts.update(10, false), 10);
        QCOMPARE(alerts.update(5, false), 0);
    }

    void startingVeryLowWarnsOnlyOnce()
    {
        BatteryAlerts alerts;
        QCOMPARE(alerts.update(8, false), 10); // not 20 and then 10
        QCOMPARE(alerts.update(7, false), 0);
    }

    void chargingRearms()
    {
        BatteryAlerts alerts;
        QCOMPARE(alerts.update(18, false), 20);
        QCOMPARE(alerts.update(18, true), 0); // never while charging
        QCOMPARE(alerts.update(18, false), 20);
    }

    void smallBouncesDoNotRepeat()
    {
        BatteryAlerts alerts;
        QCOMPARE(alerts.update(20, false), 20);
        QCOMPARE(alerts.update(21, false), 0); // readings jitter by a percent or two
        QCOMPARE(alerts.update(20, false), 0);
        QCOMPARE(alerts.update(30, false), 0); // clearly above: re-armed
        QCOMPARE(alerts.update(20, false), 20);
    }

    void tenRearmsAboveFifteen()
    {
        BatteryAlerts alerts;
        QCOMPARE(alerts.update(9, false), 10);
        QCOMPARE(alerts.update(16, false), 0);
        QCOMPARE(alerts.update(10, false), 10);
    }

    void unknownLevelIsIgnored()
    {
        BatteryAlerts alerts;
        QCOMPARE(alerts.update(-1, false), 0);
        QCOMPARE(alerts.update(15, false), 20);
    }
};

QTEST_APPLESS_MAIN(TestBatteryAlerts)
#include "test_batteryalerts.moc"
