#pragma once

#include <QSet>

// Decides when to warn about a low battery: once at 20 % and once at 10 % per
// discharge. Charging, or going clearly above a threshold, re-arms the alerts.
class BatteryAlerts
{
public:
    // Feed every new reading; returns the threshold to warn about, or 0.
    int update(int level, bool charging)
    {
        if (level < 0)
            return 0;
        if (charging || level > 25)
            m_alerted.clear();
        else if (level > 15)
            m_alerted.remove(10);
        if (charging)
            return 0;
        for (const int threshold : {10, 20}) {
            if (level <= threshold && !m_alerted.contains(threshold)) {
                m_alerted.insert(threshold);
                if (threshold == 10)
                    m_alerted.insert(20); // no 20 % alert right after a 10 % one
                return threshold;
            }
        }
        return 0;
    }

private:
    QSet<int> m_alerted;
};
