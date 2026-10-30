#pragma once

#include <QObject>
#include <QPointer>
#include <QSystemTrayIcon>
#include <QtQml/qqmlregistration.h>

#include "batteryalerts.h"

class HeadsetController;
class QJSEngine;
class QMenu;
class QQmlEngine;
class QQuickWindow;
class QAction;

// Desktop integration: tray icon with battery level, low-battery notifications,
// start at login, keep running when the window is closed, and single instance
// (a second launch just shows the running window, via the session D-Bus).
class AppController : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.github.J5MX.ArctisControl")
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool trayAvailable READ trayAvailable CONSTANT)
    Q_PROPERTY(bool startMinimized READ startMinimized CONSTANT)
    // Wayland does not tell apps when they are minimized, so "minimize to tray"
    // only works on X11. Elsewhere the header has a "Hide to tray" button.
    Q_PROPERTY(bool canDetectMinimize READ canDetectMinimize CONSTANT)
    Q_PROPERTY(bool closeToTray READ closeToTray WRITE setCloseToTray NOTIFY settingsChanged)
    Q_PROPERTY(bool minimizeToTray READ minimizeToTray WRITE setMinimizeToTray NOTIFY settingsChanged)
    // Whether the app keeps running in the background (polling for the tray).
    Q_PROPERTY(bool trayActive READ trayActive NOTIFY settingsChanged)
    Q_PROPERTY(bool startAtLogin READ startAtLogin WRITE setStartAtLogin NOTIFY settingsChanged)
    Q_PROPERTY(bool lowBatteryAlerts READ lowBatteryAlerts WRITE setLowBatteryAlerts NOTIFY settingsChanged)

public:
    // Created in main() before the QML engine, then handed to QML as a singleton.
    static AppController *instance();
    static AppController *create(QQmlEngine *, QJSEngine *);

    explicit AppController(bool startMinimized, QObject *parent = nullptr);
    ~AppController() override;

    // Returns false if another instance already runs (it has been asked to show itself).
    bool claimSingleInstance();
    void attach(QQuickWindow *window, HeadsetController *headset);

    bool trayAvailable() const;
    bool startMinimized() const { return m_startMinimized && trayAvailable(); }
    bool canDetectMinimize() const;
    bool closeToTray() const { return m_closeToTray; }
    void setCloseToTray(bool enabled);
    bool minimizeToTray() const { return m_minimizeToTray; }
    void setMinimizeToTray(bool enabled);
    bool trayActive() const { return trayAvailable() && (m_closeToTray || m_minimizeToTray); }
    bool startAtLogin() const;
    void setStartAtLogin(bool enabled);
    bool lowBatteryAlerts() const { return m_lowBatteryAlerts; }
    void setLowBatteryAlerts(bool enabled);

    // Called by QML when the window is hidden to the tray.
    Q_INVOKABLE void hiddenToTray();

public slots:
    // Exported on D-Bus: a second launch calls it.
    Q_SCRIPTABLE void show();

signals:
    void settingsChanged();

private:
    void toggleWindow();
    void updateTray();
    void checkBattery();
    void notify(const QString &summary, const QString &body);
    void saveSettings() const;

    bool m_startMinimized = false;
    bool m_closeToTray = true;
    bool m_minimizeToTray = false;
    bool m_lowBatteryAlerts = true;
    bool m_toldAboutTray = false;
    BatteryAlerts m_alerts;

    QPointer<QQuickWindow> m_window;
    QPointer<HeadsetController> m_headset;
    QSystemTrayIcon *m_tray = nullptr;
    QMenu *m_menu = nullptr;
    QAction *m_batteryAction = nullptr;
    QAction *m_toggleAction = nullptr;
};
