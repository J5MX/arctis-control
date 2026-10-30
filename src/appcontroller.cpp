#include "appcontroller.h"

#include "headsetcontroller.h"

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QMenu>
#include <QPainter>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>

namespace {

const QString kService = QStringLiteral("io.github.J5MX.ArctisControl");
const QString kInterface = QStringLiteral("io.github.J5MX.ArctisControl");
const QString kIconPath = QStringLiteral(":/qt/qml/ArctisControl/assets/icon.svg");

AppController *s_instance = nullptr;

QString configPath(const QString &relative)
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QLatin1Char('/') + relative;
}

QString settingsFile() { return configPath(QStringLiteral("arctis-control/settings.ini")); }
QString autostartFile() { return configPath(QStringLiteral("autostart/arctis-control.desktop")); }

// Quoting for the Exec key of a .desktop file.
QString desktopExecQuote(const QString &path)
{
    static const QString special = QStringLiteral(" \t\"'\\><~|&;$*?#()`");
    bool needsQuotes = false;
    for (const QChar c : path)
        needsQuotes |= special.contains(c);
    if (!needsQuotes)
        return path;
    QString escaped;
    for (const QChar c : path) {
        if (c == QLatin1Char('"') || c == QLatin1Char('`') || c == QLatin1Char('$') || c == QLatin1Char('\\'))
            escaped += QLatin1Char('\\');
        escaped += c;
    }
    return QLatin1Char('"') + escaped + QLatin1Char('"');
}

} // namespace

AppController *AppController::instance()
{
    return s_instance;
}

AppController *AppController::create(QQmlEngine *, QJSEngine *)
{
    QQmlEngine::setObjectOwnership(s_instance, QQmlEngine::CppOwnership);
    return s_instance;
}

AppController::AppController(bool startMinimized, QObject *parent)
    : QObject(parent)
    , m_startMinimized(startMinimized)
{
    s_instance = this;
    const QSettings settings(settingsFile(), QSettings::IniFormat);
    // "keepInTray" is the name used by 0.6.0 development builds.
    m_closeToTray = settings.value(QStringLiteral("closeToTray"),
                                   settings.value(QStringLiteral("keepInTray"), true)).toBool();
    m_minimizeToTray = settings.value(QStringLiteral("minimizeToTray"), false).toBool();
    m_lowBatteryAlerts = settings.value(QStringLiteral("lowBatteryAlerts"), true).toBool();
    m_toldAboutTray = settings.value(QStringLiteral("toldAboutTray"), false).toBool();
}

AppController::~AppController()
{
    delete m_menu; // a QMenu cannot have a QObject parent
    if (s_instance == this)
        s_instance = nullptr;
}

bool AppController::claimSingleInstance()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected())
        return true; // no session bus: just run
    if (!bus.registerService(kService)) {
        // Already running: ask that instance to show its window and leave.
        const QDBusMessage call = QDBusMessage::createMethodCall(kService, QStringLiteral("/"), kInterface,
                                                                 QStringLiteral("show"));
        bus.call(call, QDBus::Block, 2000);
        return false;
    }
    bus.registerObject(QStringLiteral("/"), this, QDBusConnection::ExportScriptableSlots);
    return true;
}

void AppController::attach(QQuickWindow *window, HeadsetController *headset)
{
    m_window = window;
    m_headset = headset;
    if (headset) {
        connect(headset, &HeadsetController::statusChanged, this, [this] {
            updateTray();
            checkBattery();
        });
        connect(headset, &HeadsetController::deviceChanged, this, &AppController::updateTray);
    }
    if (window)
        connect(window, &QWindow::visibleChanged, this, &AppController::updateTray);

    if (trayAvailable()) {
        m_menu = new QMenu;
        m_toggleAction = m_menu->addAction(QString(), this, &AppController::toggleWindow);
        m_batteryAction = m_menu->addAction(QString());
        m_batteryAction->setEnabled(false);
        m_menu->addSeparator();
        m_menu->addAction(QIcon::fromTheme(QStringLiteral("application-exit")), tr("Quit"),
                          qApp, &QCoreApplication::quit);

        m_tray = new QSystemTrayIcon(this);
        m_tray->setContextMenu(m_menu);
        connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
            if (reason == QSystemTrayIcon::Trigger)
                toggleWindow();
        });
    }
    updateTray();
}

bool AppController::trayAvailable() const
{
    return QSystemTrayIcon::isSystemTrayAvailable();
}

bool AppController::canDetectMinimize() const
{
    return QGuiApplication::platformName() == QLatin1String("xcb");
}

void AppController::setCloseToTray(bool enabled)
{
    if (enabled == m_closeToTray)
        return;
    m_closeToTray = enabled;
    saveSettings();
    updateTray();
    emit settingsChanged();
}

void AppController::setMinimizeToTray(bool enabled)
{
    if (enabled == m_minimizeToTray)
        return;
    m_minimizeToTray = enabled;
    saveSettings();
    updateTray();
    emit settingsChanged();
}

bool AppController::startAtLogin() const
{
    return QFile::exists(autostartFile());
}

void AppController::setStartAtLogin(bool enabled)
{
    if (enabled == startAtLogin())
        return;
    if (enabled) {
        // Installed builds use the name in PATH; development builds their own path.
        const QString binary = QCoreApplication::applicationFilePath();
        const QString exec = binary.startsWith(QLatin1String("/usr/")) ? QStringLiteral("arctis-control")
                                                                       : desktopExecQuote(binary);
        QDir().mkpath(QFileInfo(autostartFile()).absolutePath());
        QSaveFile file(autostartFile());
        if (file.open(QIODevice::WriteOnly)) {
            file.write(QStringLiteral("[Desktop Entry]\n"
                                      "Type=Application\n"
                                      "Name=Arctis Control\n"
                                      "Comment=Starts Arctis Control in the tray\n"
                                      "Exec=%1 --minimized\n"
                                      "Icon=arctis-control\n"
                                      "Terminal=false\n"
                                      "X-GNOME-Autostart-enabled=true\n")
                           .arg(exec)
                           .toUtf8());
            file.commit();
        }
    } else {
        QFile::remove(autostartFile());
    }
    emit settingsChanged();
}

void AppController::setLowBatteryAlerts(bool enabled)
{
    if (enabled == m_lowBatteryAlerts)
        return;
    m_lowBatteryAlerts = enabled;
    saveSettings();
    emit settingsChanged();
}

void AppController::hiddenToTray()
{
    if (m_toldAboutTray)
        return;
    m_toldAboutTray = true;
    saveSettings();
    notify(tr("Arctis Control is still running"),
           tr("It keeps watching the battery from the tray. Use Quit in the tray menu to close it."));
}

void AppController::show()
{
    if (!m_window)
        return;
    m_window->showNormal();
    m_window->raise();
    m_window->requestActivate();
}

void AppController::toggleWindow()
{
    if (!m_window)
        return;
    if (m_window->isVisible() && m_window->visibility() != QWindow::Minimized)
        m_window->hide();
    else
        show();
}

void AppController::updateTray()
{
    // Closing is handled explicitly in QML (quit or hide), and a window hidden to
    // the tray must never end the app on its own.
    QApplication::setQuitOnLastWindowClosed(!m_tray);
    if (!m_tray)
        return;
    // Visible when an option uses it, and always while the window is hidden,
    // so there is always a way back to it.
    m_tray->setVisible(trayActive() || (m_window && !m_window->isVisible()));

    const bool connected = m_headset && m_headset->connected();
    const int level = connected ? m_headset->batteryLevel() : -1;
    const bool charging = connected && m_headset->batteryStatus() == QLatin1String("BATTERY_CHARGING");

    QString battery;
    if (!connected)
        battery = tr("No headset detected");
    else if (level < 0)
        battery = tr("Headset off or out of range");
    else
        battery = (charging ? tr("Charging: %1 %") : tr("Battery: %1 %")).arg(level);

    m_tray->setToolTip(connected ? QStringLiteral("%1\n%2").arg(m_headset->deviceName(), battery)
                                 : QStringLiteral("Arctis Control\n%1").arg(battery));
    if (m_batteryAction)
        m_batteryAction->setText(battery);
    if (m_toggleAction)
        m_toggleAction->setText(m_window && m_window->isVisible() ? tr("Hide window") : tr("Open Arctis Control"));

    // App icon with a battery bar along the bottom edge.
    QPixmap pixmap = QIcon(kIconPath).pixmap(64, 64);
    if (level >= 0) {
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        const QRectF bar(8, 50, 48, 9);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0x22, 0x22, 0x28));
        painter.drawRoundedRect(bar, 3, 3);
        const QColor fill = charging ? QColor(0xff, 0x7a, 0x33)
                          : level <= 15 ? QColor(0xff, 0x3b, 0x3b)
                          : level <= 35 ? QColor(0xff, 0xb0, 0x20)
                          : QColor(0xff, 0x52, 0x00);
        painter.setBrush(fill);
        painter.drawRoundedRect(QRectF(bar.x(), bar.y(), qMax(3.0, bar.width() * level / 100.0), bar.height()), 3, 3);
    }
    m_tray->setIcon(QIcon(pixmap));
}

void AppController::checkBattery()
{
    if (!m_headset || !m_headset->connected())
        return;
    const int level = m_headset->batteryLevel();
    const bool charging = m_headset->batteryStatus() == QLatin1String("BATTERY_CHARGING");
    if (m_alerts.update(level, charging) > 0 && m_lowBatteryAlerts)
        notify(tr("%1: low battery").arg(m_headset->deviceName()),
               tr("%1 % left. Charge the headset soon.").arg(level));
}

void AppController::notify(const QString &summary, const QString &body)
{
    QDBusMessage message = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.Notifications"), QStringLiteral("/org/freedesktop/Notifications"),
        QStringLiteral("org.freedesktop.Notifications"), QStringLiteral("Notify"));
    const QVariantMap hints{{QStringLiteral("desktop-entry"), QStringLiteral("arctis-control")}};
    message << QStringLiteral("Arctis Control") << uint(0) << QStringLiteral("arctis-control") << summary << body
            << QStringList() << hints << int(-1);
    QDBusConnection::sessionBus().asyncCall(message); // fire and forget: never blocks the UI
}

void AppController::saveSettings() const
{
    QSettings settings(settingsFile(), QSettings::IniFormat);
    settings.setValue(QStringLiteral("closeToTray"), m_closeToTray);
    settings.setValue(QStringLiteral("minimizeToTray"), m_minimizeToTray);
    settings.remove(QStringLiteral("keepInTray"));
    settings.setValue(QStringLiteral("lowBatteryAlerts"), m_lowBatteryAlerts);
    settings.setValue(QStringLiteral("toldAboutTray"), m_toldAboutTray);
}
