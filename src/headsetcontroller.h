#pragma once

#include <QJsonObject>
#include <QObject>
#include <QProcess>
#include <QQueue>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <functional>

// Puente entre la interfaz QML y HeadsetControl. Ejecuta /usr/bin/headsetcontrol
// con argumentos validados (sin shell) y de uno en uno.
class HeadsetController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool connected READ connected NOTIFY statusChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY statusChanged)
    Q_PROPERTY(int batteryLevel READ batteryLevel NOTIFY statusChanged)
    Q_PROPERTY(QString batteryStatus READ batteryStatus NOTIFY statusChanged)
    Q_PROPERTY(QString updatedText READ updatedText NOTIFY statusChanged)
    Q_PROPERTY(QVariantList presets READ presets NOTIFY presetsChanged)
    Q_PROPERTY(int eqPreset READ eqPreset NOTIFY stateChanged)
    Q_PROPERTY(QVariantList eqCustom READ eqCustom NOTIFY stateChanged)
    Q_PROPERTY(int volumeLimiter READ volumeLimiter NOTIFY stateChanged)
    Q_PROPERTY(int inactiveTime READ inactiveTime NOTIFY stateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)

public:
    explicit HeadsetController(QObject *parent = nullptr);

    bool connected() const { return m_connected; }
    QString errorText() const { return m_errorText; }
    int batteryLevel() const { return m_batteryLevel; }
    QString batteryStatus() const { return m_batteryStatus; }
    QString updatedText() const { return m_updatedText; }
    QVariantList presets() const { return m_presets; }
    int eqPreset() const { return m_eqPreset; }
    QVariantList eqCustom() const;
    int volumeLimiter() const { return m_volumeLimiter; }
    int inactiveTime() const { return m_inactiveTime; }
    bool busy() const { return m_running || !m_queue.isEmpty(); }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void setEqPreset(int index);
    Q_INVOKABLE void setEqCustom(const QVariantList &bands);
    Q_INVOKABLE void setVolumeLimiter(bool enabled);
    Q_INVOKABLE void setInactiveTime(int minutes);

signals:
    void statusChanged();
    void presetsChanged();
    void stateChanged();
    void busyChanged();
    void notify(const QString &message, bool error);

private:
    struct Job {
        QStringList args;
        std::function<void()> onSuccess;
        QString successMessage;
        bool isRefresh = false;
    };

    void enqueue(Job job);
    void runNext();
    void handleFinished(const QByteArray &output, const QString &failure);
    void applyStatus(const QJsonObject &device, const QByteArray &rawOutput);
    void setDisconnected(const QString &reason);
    void loadState();
    void saveState() const;

    QProcess m_process;
    QTimer m_killTimer;
    QTimer m_pollTimer;
    QQueue<Job> m_queue;
    Job m_current;
    bool m_running = false;

    bool m_connected = false;
    QString m_errorText;
    int m_batteryLevel = -1;
    QString m_batteryStatus;
    QString m_updatedText;
    QVariantList m_presets;

    // Último valor aplicado; HeadsetControl no permite leerlos del auricular.
    int m_eqPreset = -1;
    QList<double> m_eqCustom = QList<double>(10, 0.0);
    int m_volumeLimiter = -1; // -1 = desconocido
    int m_inactiveTime = 5;
};
