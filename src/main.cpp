#include <QCommandLineParser>
#include <QGuiApplication>
#include <QIcon>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QQmlApplicationEngine>
#include <QQmlNetworkAccessManagerFactory>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>

#include "headsetcontroller.h"

#include <cstdio>
#include <functional>

namespace {

// La app no necesita red. Si algún texto o imagen intentara cargar una URL,
// la petición falla sin salir del equipo: solo se permiten recursos qrc.
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

// Prueba automática contra el auricular simulado: aplica cada ajuste con el
// mismo código que usa la interfaz y comprueba que headsetcontrol lo acepta.
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
            bands.first() = controller.eqMin(); // extremo inferior del rango
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
        std::printf("%-4s %-16s %s\n", error ? "MAL" : "OK", qPrintable(steps.at(index).name), qPrintable(message));
        next();
    });
    QObject::connect(&controller, &HeadsetController::deviceChanged, &app, [&] {
        if (index >= 0 || !controller.connected())
            return;
        std::printf("Dispositivo: %s (%s)\n", qPrintable(controller.deviceName()), qPrintable(controller.deviceId()));
        next();
    });
    QTimer::singleShot(60000, &app, [] { std::printf("Tiempo agotado\n"); QCoreApplication::exit(99); });
    return QGuiApplication::exec();
}

} // namespace

int main(int argc, char *argv[])
{
    // Estilo Basic: es el que permite personalizar por completo los controles.
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("arctis-control"));
    QGuiApplication::setApplicationVersion(QStringLiteral(APP_VERSION));
    // Enlaza la ventana con arctis-control.desktop (icono en la barra de tareas en Wayland).
    QGuiApplication::setDesktopFileName(QStringLiteral("arctis-control"));
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/qt/qml/ArctisControl/assets/icon.svg")));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Panel para auriculares SteelSeries Arctis (vía HeadsetControl)"));
    parser.addHelpOption();
    parser.addVersionOption();
    // Para desarrollo: guarda una captura de la ventana y sale. Útil con QT_QPA_PLATFORM=offscreen.
    const QCommandLineOption screenshot(QStringLiteral("screenshot"),
                                        QStringLiteral("Guarda una captura PNG de la ventana y sale."),
                                        QStringLiteral("archivo"));
    parser.addOption(screenshot);
    // Para desarrollo: usa el auricular simulado de HeadsetControl, que tiene todas las funciones.
    const QCommandLineOption testDevice(QStringLiteral("test-device"),
                                        QStringLiteral("Usa el auricular simulado de HeadsetControl."));
    parser.addOption(testDevice);
    const QCommandLineOption selfTest(QStringLiteral("self-test"),
                                      QStringLiteral("Con --test-device: aplica todos los ajustes al simulado y sale."));
    parser.addOption(selfTest);
    parser.process(app);
    HeadsetController::setTestDevice(parser.isSet(testDevice));
    if (parser.isSet(selfTest)) {
        if (!parser.isSet(testDevice)) {
            std::fprintf(stderr, "--self-test solo funciona junto a --test-device\n");
            return 2;
        }
        return runSelfTest(app);
    }

    QQmlApplicationEngine engine;
    OfflineFactory offline;
    engine.setNetworkAccessManagerFactory(&offline);
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("ArctisControl", "Main");

    if (parser.isSet(screenshot) && !engine.rootObjects().isEmpty()) {
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
        const QString path = parser.value(screenshot);
        window->resize(1000, 1500); // que quepa todo en la captura
        QTimer::singleShot(3000, &app, [window, path] {
            QCoreApplication::exit(window && window->grabWindow().save(path) ? 0 : 1);
        });
    }

    return QGuiApplication::exec();
}
