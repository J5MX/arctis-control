#include <QCommandLineParser>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>

int main(int argc, char *argv[])
{
    // Estilo Basic: es el que permite personalizar por completo los controles.
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("nova5-panel"));
    QGuiApplication::setApplicationVersion(QStringLiteral(NOVA5_VERSION));
    // Enlaza la ventana con nova5-panel.desktop (icono en la barra de tareas en Wayland).
    QGuiApplication::setDesktopFileName(QStringLiteral("nova5-panel"));
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/qt/qml/Nova5Panel/assets/icon.svg")));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Panel para los SteelSeries Arctis Nova 5"));
    parser.addHelpOption();
    parser.addVersionOption();
    // Para desarrollo: guarda una captura de la ventana y sale. Útil con QT_QPA_PLATFORM=offscreen.
    const QCommandLineOption screenshot(QStringLiteral("screenshot"),
                                        QStringLiteral("Guarda una captura PNG de la ventana y sale."),
                                        QStringLiteral("archivo"));
    parser.addOption(screenshot);
    parser.process(app);

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("Nova5Panel", "Main");

    if (parser.isSet(screenshot) && !engine.rootObjects().isEmpty()) {
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
        const QString path = parser.value(screenshot);
        QTimer::singleShot(3000, &app, [window, path] {
            QCoreApplication::exit(window && window->grabWindow().save(path) ? 0 : 1);
        });
    }

    return QGuiApplication::exec();
}
