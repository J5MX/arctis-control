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

} // namespace

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
    OfflineFactory offline;
    engine.setNetworkAccessManagerFactory(&offline);
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
