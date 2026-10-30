#pragma once

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QProcess>
#include <QQueue>
#include <QStringList>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

#include <functional>

// Bridge between the QML UI and HeadsetControl. Detects the connected SteelSeries
// headsets, exposes what each one supports and runs /usr/bin/headsetcontrol with
// validated arguments (no shell), one command at a time.
class HeadsetController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool connected READ connected NOTIFY statusChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY statusChanged)
    Q_PROPERTY(QStringList deviceNames READ deviceNames NOTIFY devicesChanged)
    Q_PROPERTY(int deviceCount READ deviceCount NOTIFY devicesChanged)
    Q_PROPERTY(int currentDevice READ currentDevice WRITE setCurrentDevice NOTIFY deviceChanged)
    Q_PROPERTY(QString deviceName READ deviceName NOTIFY deviceChanged)
    Q_PROPERTY(QString deviceId READ deviceId NOTIFY deviceChanged)
    Q_PROPERTY(QStringList capabilities READ capabilities NOTIFY deviceChanged)

    Q_PROPERTY(int batteryLevel READ batteryLevel NOTIFY statusChanged)
    Q_PROPERTY(QString batteryStatus READ batteryStatus NOTIFY statusChanged)
    Q_PROPERTY(int batteryMinutes READ batteryMinutes NOTIFY statusChanged)
    Q_PROPERTY(int chatmix READ chatmix NOTIFY statusChanged)
    Q_PROPERTY(QString updatedText READ updatedText NOTIFY statusChanged)

    Q_PROPERTY(QStringList presetNames READ presetNames NOTIFY deviceChanged)
    Q_PROPERTY(int presetCount READ presetCount NOTIFY deviceChanged)
    Q_PROPERTY(int eqBands READ eqBands NOTIFY deviceChanged)
    Q_PROPERTY(double eqMin READ eqMin NOTIFY deviceChanged)
    Q_PROPERTY(double eqMax READ eqMax NOTIFY deviceChanged)
    Q_PROPERTY(double eqStep READ eqStep NOTIFY deviceChanged)
    Q_PROPERTY(int eqPreset READ eqPreset NOTIFY stateChanged)
    // Bands shown by the equalizer: the active preset's or the custom curve.
    Q_PROPERTY(QList<double> activeBands READ activeBands NOTIFY stateChanged)

    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    // The UI turns it off while minimized so no processes are spawned for nothing.
    Q_PROPERTY(bool polling READ polling WRITE setPolling NOTIFY pollingChanged)
    // Hidden in the tray: poll less often (only the battery matters there).
    Q_PROPERTY(bool background READ background WRITE setBackground NOTIFY backgroundChanged)

public:
    explicit HeadsetController(QObject *parent = nullptr);
    ~HeadsetController() override;

    // Development mode: use HeadsetControl's mock device.
    static void setTestDevice(bool enabled);

    bool connected() const { return m_current >= 0; }
    QString errorText() const { return m_errorText; }
    QStringList deviceNames() const;
    int deviceCount() const { return int(m_devices.size()); }
    int currentDevice() const { return m_current; }
    void setCurrentDevice(int index);
    QString deviceName() const;
    QString deviceId() const;
    QStringList capabilities() const;

    int batteryLevel() const;
    QString batteryStatus() const;
    int batteryMinutes() const;
    int chatmix() const;
    QString updatedText() const { return m_updatedText; }

    QStringList presetNames() const;
    int presetCount() const { return int(presetNames().size()); }
    int eqBands() const;
    double eqMin() const;
    double eqMax() const;
    double eqStep() const;
    int eqPreset() const;
    QList<double> activeBands() const;

    bool busy() const { return m_running || !m_queue.isEmpty(); }
    bool polling() const { return m_pollTimer.isActive(); }
    void setPolling(bool enabled);
    bool background() const { return m_background; }
    void setBackground(bool background);

    Q_INVOKABLE void refresh();
    // Last applied value of a simple setting (or the one read from the headset); -1 if unknown.
    Q_INVOKABLE int setting(const QString &key) const;
    Q_INVOKABLE void apply(const QString &key, int value);
    Q_INVOKABLE void setEqPreset(int index);
    Q_INVOKABLE void setEqCustom(const QList<double> &bands);
    Q_INVOKABLE void playNotification();

signals:
    void statusChanged();
    void devicesChanged();
    void deviceChanged();
    void stateChanged();
    void busyChanged();
    void pollingChanged();
    void backgroundChanged();
    void notify(const QString &message, bool error);

private:
    struct Device {
        QString id;       // "1038:2232": key of the saved settings
        QString selector; // "0x1038:0x2232": the format "headsetcontrol -d" requires
        QString name;     // "Arctis Nova (5/5X)"
        QStringList caps;
        QJsonObject status;
        QStringList presetNames;
        QList<QList<double>> presetBands;
        // Equalizer geometry, validated once when the device is detected.
        int eqBands = 0;
        double eqMin = -10;
        double eqMax = 10;
        double eqStep = 1;
    };

    struct Job {
        QStringList args;
        std::function<void(QJsonObject &state)> onSuccess;
        QString successMessage;
        bool isRefresh = false;
        QString deviceId; // headset whose saved state onSuccess updates
    };

    const Device *device() const;
    QStringList deviceArgs() const;
    void enqueue(Job job);
    void runNext();
    void handleFinished(const QByteArray &output, const QString &failure);
    void applyDevices(const QJsonArray &devices, const QByteArray &rawOutput);
    void setNoDevice(const QString &reason);
    void loadState();
    void saveState() const;

    QProcess m_process;
    QTimer m_killTimer;
    QTimer m_pollTimer;
    QQueue<Job> m_queue;
    Job m_job;
    bool m_running = false;

    QList<Device> m_devices;
    int m_current = -1;
    QString m_errorText;
    QString m_updatedText;
    bool m_background = false;

    // Last applied values per device ("1038:2232" → {key: value}).
    // HeadsetControl cannot read most settings back from the headset.
    QHash<QString, QJsonObject> m_state;
};
