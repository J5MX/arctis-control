#pragma once

#include <QObject>
#include <QProcess>
#include <QStringList>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

// Turns the headset's own microphone on or off at the PipeWire level, for people
// who use a separate microphone. HeadsetControl cannot do this: it switches the
// headset's sound card between an output+input profile and the matching
// output-only profile (WirePlumber remembers the choice). Runs /usr/bin/pactl.
class HeadsetMic : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // Headset to look for, as "1038:2232" (HeadsetController.deviceId).
    Q_PROPERTY(QString usbId READ usbId WRITE setUsbId NOTIFY usbIdChanged)
    // A sound card for the headset exists and has profiles with and without input.
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(bool enabled READ enabled NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(bool polling READ polling WRITE setPolling NOTIFY pollingChanged)

public:
    explicit HeadsetMic(QObject *parent = nullptr);
    ~HeadsetMic() override;

    QString usbId() const { return m_usbId; }
    void setUsbId(const QString &id);
    bool available() const { return m_available; }
    bool enabled() const { return m_enabled; }
    bool busy() const { return m_process.state() != QProcess::NotRunning; }
    bool polling() const { return m_pollTimer.isActive(); }
    void setPolling(bool enabled);

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void setEnabled(bool enabled);

signals:
    void usbIdChanged();
    void changed();
    void busyChanged();
    void pollingChanged();
    void cardsLoaded(); // after every read of the card list, even a failed one
    void notify(const QString &message, bool error);

private:
    void run(const QStringList &args, bool isSet);
    void handleFinished(const QByteArray &output, bool ok);
    void parseCards(const QByteArray &json);

    QProcess m_process;
    QTimer m_killTimer;
    QTimer m_pollTimer;
    bool m_pendingSet = false;
    bool m_targetEnabled = false;

    QString m_usbId;
    bool m_available = false;
    bool m_enabled = false;
    QString m_card;
    QString m_profileWithMic;    // best profile to switch to when enabling
    QString m_profileWithoutMic; // best profile to switch to when disabling
};
