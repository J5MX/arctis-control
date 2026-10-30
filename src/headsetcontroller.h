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

// Puente entre la interfaz QML y HeadsetControl. Detecta los auriculares
// SteelSeries conectados, expone lo que admite cada uno y ejecuta
// /usr/bin/headsetcontrol con argumentos validados (sin shell), de uno en uno.
class HeadsetController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool connected READ connected NOTIFY statusChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY statusChanged)
    Q_PROPERTY(QStringList deviceNames READ deviceNames NOTIFY devicesChanged)
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
    Q_PROPERTY(int eqBands READ eqBands NOTIFY deviceChanged)
    Q_PROPERTY(double eqMin READ eqMin NOTIFY deviceChanged)
    Q_PROPERTY(double eqMax READ eqMax NOTIFY deviceChanged)
    Q_PROPERTY(double eqStep READ eqStep NOTIFY deviceChanged)
    Q_PROPERTY(int eqPreset READ eqPreset NOTIFY stateChanged)
    // Bandas que muestra el ecualizador: las del preset activo o la curva personalizada.
    Q_PROPERTY(QList<double> activeBands READ activeBands NOTIFY stateChanged)

    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    // La interfaz lo desactiva con la ventana minimizada para no lanzar procesos en balde.
    Q_PROPERTY(bool polling READ polling WRITE setPolling NOTIFY pollingChanged)

public:
    explicit HeadsetController(QObject *parent = nullptr);

    // Modo de desarrollo: usa el dispositivo simulado de HeadsetControl.
    static void setTestDevice(bool enabled);

    bool connected() const { return m_current >= 0; }
    QString errorText() const { return m_errorText; }
    QStringList deviceNames() const;
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
    int eqBands() const;
    double eqMin() const;
    double eqMax() const;
    double eqStep() const;
    int eqPreset() const;
    QList<double> activeBands() const;

    bool busy() const { return m_running || !m_queue.isEmpty(); }
    bool polling() const { return m_pollTimer.isActive(); }
    void setPolling(bool enabled);

    Q_INVOKABLE void refresh();
    // Último valor aplicado de un ajuste simple (o el leído del auricular); -1 si no se sabe.
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
    void notify(const QString &message, bool error);

private:
    struct Device {
        QString id;       // "1038:2232": clave de los ajustes guardados
        QString selector; // "0x1038:0x2232": formato que exige «headsetcontrol -d»
        QString name; // "Arctis Nova (5/5X)"
        QStringList caps;
        QJsonObject status;
        QStringList presetNames;
        QList<QList<double>> presetBands;
    };

    struct Job {
        QStringList args;
        std::function<void(QJsonObject &state)> onSuccess;
        QString successMessage;
        bool isRefresh = false;
    };

    const Device *device() const;
    QStringList deviceArgs() const;
    void enqueue(Job job);
    void runNext();
    void handleFinished(const QByteArray &output, const QString &failure);
    void applyDevices(const QJsonArray &devices, const QByteArray &rawOutput);
    void setNoDevice(const QString &reason);
    QJsonObject &currentState();
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

    // Últimos valores aplicados por dispositivo ("1038:2232" → {clave: valor}).
    // HeadsetControl no permite leer la mayoría de ajustes del auricular.
    QHash<QString, QJsonObject> m_state;
};
