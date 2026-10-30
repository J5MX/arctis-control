#include <QCommandLineParser>
#include <QGuiApplication>
#include <QIcon>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QQmlApplicationEngine>
#include <QQmlNetworkAccessManagerFactory>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#include <QTranslator>

#include "headsetcontroller.h"

#include <cstdio>
#include <functional>

namespace {

// The app needs no network. If any text or image tried to load a URL, the
// request fails without leaving the machine: only qrc resources are allowed.
class OfflineAccessManager : public QNetworkAccessManager
{
public:
    using QNetworkAccessManager::QNetworkAccessManager;

protected:
    QNetworkReply *createRequest(Operation op, const QNetworkRequest &request, QIODevice *data) override
    {
        if (request.url().scheme() == QLatin1String("qrc"))
            return QNetworkAccessManager::createRequest(op, request, data);
        return QNetworkAccessManager::createRequest(op, QNetworkRequest(QUrl()), data);
    }
};

class OfflineFactory : public QQmlNetworkAccessManagerFactory
{
public:
    QNetworkAccessManager *create(QObject *parent) override { return new OfflineAccessManager(parent); }
};

// Automated test against the mock headset: applies every setting through the
// same code the UI uses and checks that headsetcontrol accepts it.
int runSelfTest(QGuiApplication &app)
{
    HeadsetController controller;
    struct Step { QString name; std::function<void()> run; };
    QList<Step> steps;
    const char *keys[] = {"sidetone", "lights", "inactive_time", "voice_prompts", "rotate_to_mute", "mic_led",
                          "mic_volume", "volume_limiter", "bt_powered_on", "bt_call_volume", "noise_filter"};
    for (const char *key : keys)
        steps.append({QString::fromLatin1(key), [&controller, key] { controller.apply(QString::fromLatin1(key), 1); }});
    steps.append({QStringLiteral("eq_preset"), [&controller] { controller.setEqPreset(1); }});
    steps.append({QStringLiteral("eq_custom"), [&controller] {
        QList<double> bands(controller.eqBands(), 0.0);
        if (!bands.isEmpty())
            bands.first() = controller.eqMin(); // lower end of the range
        controller.setEqCustom(bands);
    }});
    steps.append({QStringLiteral("notification"), [&controller] { controller.playNotification(); }});

    int index = -1;
    int failures = 0;
    auto next = [&] {
        if (++index >= steps.size())
            return QCoreApplication::exit(failures);
        steps.at(index).run();
    };
    QObject::connect(&controller, &HeadsetController::notify, &app, [&](const QString &message, bool error) {
        if (index < 0)
            return;
        failures += error ? 1 : 0;
        std::printf("%-4s %-16s %s\n", error ? "FAIL" : "OK", qPrintable(steps.at(index).name), qPrintable(message));
        next();
    });
    QObject::connect(&controller, &HeadsetController::deviceChanged, &app, [&] {
        if (index >= 0 || !controller.connected())
            return;
        std::printf("Device: %s (%s)\n", qPrintable(controller.deviceName()), qPrintable(controller.deviceId()));
        next();
    });
    QTimer::singleShot(60000, &app, [] { std::printf("Timed out\n"); QCoreApplication::exit(99); });
    return QGuiApplication::exec();
}

} // namespace

int main(int argc, char *argv[])
{
    // The Basic style is the one that allows fully customizing the controls.
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QGuiApplication app(argc, argv);

    // UI in English; translated to the system language when available (e.g. Spanish).
    QTranslator translator;
    if (translator.load(QLocale(), QStringLiteral("arctis-control"), QStringLiteral("_"), QStringLiteral(":/i18n")))
        QCoreApplication::installTranslator(&translator);
    QGuiApplication::setApplicationName(QStringLiteral("arctis-control"));
    QGuiApplication::setApplicationVersion(QStringLiteral(APP_VERSION));
    // Links the window to arctis-control.desktop (taskbar icon on Wayland).
    QGuiApplication::setDesktopFileName(QStringLiteral("arctis-control"));
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/qt/qml/ArctisControl/assets/icon.svg")));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Control panel for SteelSeries Arctis headsets (via HeadsetControl)"));
    parser.addHelpOption();
    parser.addVersionOption();
    // Development: save a screenshot of the window and exit. Useful with QT_QPA_PLATFORM=offscreen.
    const QCommandLineOption screenshot(QStringLiteral("screenshot"),
                                        QStringLiteral("Save a PNG screenshot of the window and exit."),
                                        QStringLiteral("file"));
    parser.addOption(screenshot);
    // Development: use HeadsetControl's mock headset, which has every feature.
    const QCommandLineOption testDevice(QStringLiteral("test-device"),
                                        QStringLiteral("Use HeadsetControl's mock headset only."));
    parser.addOption(testDevice);
    const QCommandLineOption selfTest(QStringLiteral("self-test"),
                                      QStringLiteral("With --test-device: apply every setting to the mock and exit."));
    parser.addOption(selfTest);
    parser.process(app);
    HeadsetController::setTestDevice(parser.isSet(testDevice));
    if (parser.isSet(selfTest)) {
        if (!parser.isSet(testDevice)) {
            std::fprintf(stderr, "--self-test only works together with --test-device\n");
            return 2;
        }
        return runSelfTest(app);
    }

    // Declared before the engine so it outlives it (the engine does not own it).
    OfflineFactory offline;
    QQmlApplicationEngine engine;
    engine.setNetworkAccessManagerFactory(&offline);
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("ArctisControl", "Main");

    if (parser.isSet(screenshot) && !engine.rootObjects().isEmpty()) {
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
        const QString path = parser.value(screenshot);
        window->resize(1000, 1500); // fit everything in the screenshot
        QTimer::singleShot(3000, &app, [window, path] {
            QCoreApplication::exit(window && window->grabWindow().save(path) ? 0 : 1);
        });
    }

    return QGuiApplication::exec();
}
