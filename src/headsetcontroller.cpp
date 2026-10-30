#include "headsetcontroller.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTime>

#include <cmath>
#include <utility>

namespace {

const QString kHeadsetControl = QStringLiteral("/usr/bin/headsetcontrol");
constexpr int kBands = 10;
constexpr int kTimeoutMs = 20000;
constexpr int kPollMs = 20000;
constexpr qint64 kMaxOutput = 1 << 20; // la respuesta normal ocupa ~2 KiB

QString stateDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
        + QStringLiteral("/nova5-panel");
}

QString stateFile()
{
    return stateDir() + QStringLiteral("/state.json");
}

// Los presets llegan como objeto JSON y QJsonObject ordena las claves
// alfabéticamente, pero «-p N» usa el orden original. Lo sacamos del texto.
QStringList presetOrder(const QByteArray &raw)
{
    const QString text = QString::fromUtf8(raw);
    const qsizetype start = text.indexOf(QStringLiteral("\"equalizer_presets\""));
    if (start < 0)
        return {};
    const qsizetype open = text.indexOf(QLatin1Char('{'), start);
    const qsizetype close = text.indexOf(QLatin1Char('}'), open);
    if (open < 0 || close < 0)
        return {};
    static const QRegularExpression key(QStringLiteral("\"([A-Za-z0-9_ -]+)\"\\s*:\\s*\\["));
    QStringList names;
    auto it = key.globalMatch(text.mid(open, close - open));
    while (it.hasNext())
        names << it.next().captured(1);
    return names;
}

} // namespace

HeadsetController::HeadsetController(QObject *parent)
    : QObject(parent)
{
    // Entorno mínimo: headsetcontrol no necesita nada del nuestro.
    QProcessEnvironment env;
    env.insert(QStringLiteral("LANG"), QStringLiteral("C.UTF-8"));
    m_process.setProcessEnvironment(env);
    m_process.setProgram(kHeadsetControl);

    m_killTimer.setSingleShot(true);
    m_killTimer.setInterval(kTimeoutMs);
    connect(&m_killTimer, &QTimer::timeout, this, [this] { m_process.kill(); });

    connect(&m_process, &QProcess::finished, this, [this](int, QProcess::ExitStatus status) {
        m_killTimer.stop();
        const QByteArray out = m_process.read(kMaxOutput);
        const QString err = QString::fromUtf8(m_process.readAllStandardError().left(4096)).trimmed();
        if (m_process.bytesAvailable() > 0) {
            m_process.readAll();
            return handleFinished({}, tr("Respuesta de headsetcontrol demasiado grande"));
        }
        handleFinished(out, status == QProcess::CrashExit ? tr("headsetcontrol no respondió") : err);
    });
    // Corta en cuanto la salida pasa del límite, en lugar de acumularla en memoria.
    connect(&m_process, &QProcess::readyReadStandardOutput, this, [this] {
        if (m_process.bytesAvailable() > kMaxOutput)
            m_process.kill();
    });
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            m_killTimer.stop();
            handleFinished({}, tr("Falta headsetcontrol: sudo pacman -S headsetcontrol"));
        }
    });

    m_pollTimer.setInterval(kPollMs);
    connect(&m_pollTimer, &QTimer::timeout, this, &HeadsetController::refresh);

    loadState();
    setPolling(true);
}

void HeadsetController::setPolling(bool enabled)
{
    if (enabled == m_pollTimer.isActive())
        return;
    if (enabled) {
        m_pollTimer.start();
        refresh(); // al volver a la ventana, datos al momento
    } else {
        m_pollTimer.stop();
    }
    emit pollingChanged();
}

QList<double> HeadsetController::activeBands() const
{
    if (m_eqPreset >= 0 && m_eqPreset < m_presetBands.size())
        return m_presetBands.at(m_eqPreset);
    return m_eqCustom;
}

void HeadsetController::refresh()
{
    // Evita amontonar lecturas si el auricular tarda en responder.
    if (m_current.isRefresh && m_running)
        return;
    for (const Job &job : m_queue)
        if (job.isRefresh)
            return;
    enqueue({{}, nullptr, {}, true});
}

void HeadsetController::setEqPreset(int index)
{
    if (index < 0 || index >= m_presetNames.size())
        return emit notify(tr("Preset no válido"), true);
    const QString name = m_presetNames.at(index);
    enqueue({{QStringLiteral("-p"), QString::number(index)},
             [this, index] { m_eqPreset = index; },
             tr("Preset «%1» aplicado").arg(name)});
}

void HeadsetController::setEqCustom(const QList<double> &bands)
{
    if (bands.size() != kBands)
        return emit notify(tr("El ecualizador necesita 10 bandas"), true);
    QList<double> values;
    QStringList parts;
    for (const double v : bands) {
        if (!std::isfinite(v) || v < -10.0 || v > 10.0)
            return emit notify(tr("Valor fuera de rango (-10 a 10)"), true);
        const double rounded = std::round(v * 2.0) / 2.0;
        values << rounded;
        parts << QString::number(rounded, 'g', 4);
    }
    enqueue({{QStringLiteral("-e"), parts.join(QLatin1Char(','))},
             [this, values] { m_eqCustom = values; m_eqPreset = -1; },
             tr("Ecualizador aplicado")});
}

void HeadsetController::setVolumeLimiter(bool enabled)
{
    enqueue({{QStringLiteral("--volume-limiter"), enabled ? QStringLiteral("1") : QStringLiteral("0")},
             [this, enabled] { m_volumeLimiter = enabled ? 1 : 0; },
             enabled ? tr("Limitador activado") : tr("Limitador desactivado")});
}

void HeadsetController::setInactiveTime(int minutes)
{
    if (minutes < 0 || minutes > 90)
        return emit notify(tr("Valor fuera de rango (0 a 90)"), true);
    enqueue({{QStringLiteral("-i"), QString::number(minutes)},
             [this, minutes] { m_inactiveTime = minutes; },
             minutes == 0 ? tr("Apagado automático desactivado")
                          : tr("Se apagará tras %1 min sin uso").arg(minutes)});
}

void HeadsetController::enqueue(Job job)
{
    m_queue.enqueue(std::move(job));
    emit busyChanged();
    if (!m_running)
        runNext();
}

void HeadsetController::runNext()
{
    if (m_queue.isEmpty()) {
        m_running = false;
        m_current = {};
        emit busyChanged();
        return;
    }
    m_current = m_queue.dequeue();
    m_running = true;
    m_process.setArguments(m_current.args + QStringList{QStringLiteral("-o"), QStringLiteral("json")});
    m_killTimer.start();
    m_process.start(QIODevice::ReadOnly);
}

void HeadsetController::handleFinished(const QByteArray &output, const QString &failure)
{
    const Job job = std::exchange(m_current, Job{});
    const QJsonObject root = QJsonDocument::fromJson(output).object();
    const QJsonArray devices = root.value(QStringLiteral("devices")).toArray();

    if (devices.isEmpty()) {
        const QString reason = output.isEmpty() && !failure.isEmpty()
            ? failure
            : tr("No se detecta el auricular. ¿Está el dongle conectado?");
        setDisconnected(reason);
        if (!job.isRefresh)
            emit notify(reason, true);
        return runNext();
    }

    const QJsonObject device = devices.first().toObject();
    if (job.isRefresh) {
        applyStatus(device, output);
    } else {
        const QJsonObject errors = device.value(QStringLiteral("errors")).toObject();
        if (!errors.isEmpty()) {
            QStringList messages;
            for (const QJsonValue &v : errors)
                messages << v.toString();
            emit notify(messages.join(QStringLiteral("; ")), true);
        } else {
            if (job.onSuccess)
                job.onSuccess();
            saveState();
            emit stateChanged();
            emit notify(job.successMessage, false);
        }
    }
    runNext();
}

void HeadsetController::applyStatus(const QJsonObject &device, const QByteArray &rawOutput)
{
    const QJsonObject battery = device.value(QStringLiteral("battery")).toObject();
    const QString status = battery.value(QStringLiteral("status")).toString();
    const int level = qBound(-1, battery.value(QStringLiteral("level")).toInt(-1), 100);
    const QString updated = QTime::currentTime().toString(QStringLiteral("HH:mm"));
    const bool changed = !m_connected || level != m_batteryLevel || status != m_batteryStatus
        || updated != m_updatedText;

    m_connected = true;
    m_errorText.clear();
    m_batteryStatus = status;
    m_batteryLevel = level;
    m_updatedText = updated;

    const QJsonObject presets = device.value(QStringLiteral("equalizer_presets")).toObject();
    if (m_presetNames.isEmpty() && !presets.isEmpty()) {
        const QStringList order = presetOrder(rawOutput);
        for (const QString &name : order) {
            const QJsonArray values = presets.value(name).toArray();
            if (values.size() != kBands)
                continue;
            QList<double> bands;
            for (const QJsonValue &v : values)
                bands << v.toDouble();
            m_presetNames << name;
            m_presetBands << bands;
        }
        emit presetsChanged();
        if (m_eqPreset >= 0)
            emit stateChanged(); // las bandas del preset activo ya se conocen
    }
    if (changed)
        emit statusChanged();
}

void HeadsetController::setDisconnected(const QString &reason)
{
    if (!m_connected && m_errorText == reason)
        return;
    m_connected = false;
    m_errorText = reason;
    m_batteryLevel = -1;
    m_batteryStatus.clear();
    emit statusChanged();
}

void HeadsetController::loadState()
{
    // Solo un archivo normal: un FIFO bloquearía la apertura.
    const QFileInfo info(stateFile());
    if (!info.isFile() || info.isSymLink())
        return;
    QFile file(stateFile());
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QJsonObject s = QJsonDocument::fromJson(file.read(64 * 1024)).object();

    const QJsonValue preset = s.value(QStringLiteral("eq_preset"));
    m_eqPreset = preset.isDouble() ? qBound(-1, preset.toInt(), 3) : -1;

    const QJsonArray custom = s.value(QStringLiteral("eq_custom")).toArray();
    if (custom.size() == kBands) {
        for (int i = 0; i < kBands; ++i)
            m_eqCustom[i] = qBound(-10.0, custom.at(i).toDouble(), 10.0);
    }

    const QJsonValue limiter = s.value(QStringLiteral("volume_limiter"));
    m_volumeLimiter = limiter.isBool() ? (limiter.toBool() ? 1 : 0) : -1;

    const QJsonValue inactive = s.value(QStringLiteral("inactive_time"));
    if (inactive.isDouble())
        m_inactiveTime = qBound(0, inactive.toInt(), 90);
}

void HeadsetController::saveState() const
{
    // Mismo formato que la versión web, para que ambas compartan el estado.
    QJsonArray custom;
    for (double v : m_eqCustom)
        custom << v;
    QJsonObject s{
        {QStringLiteral("eq_preset"), m_eqPreset >= 0 ? QJsonValue(m_eqPreset) : QJsonValue()},
        {QStringLiteral("eq_custom"), custom},
        {QStringLiteral("volume_limiter"), m_volumeLimiter >= 0 ? QJsonValue(m_volumeLimiter == 1) : QJsonValue()},
        {QStringLiteral("inactive_time"), m_inactiveTime},
    };

    QDir().mkpath(stateDir());
    QFile::setPermissions(stateDir(), QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    QSaveFile file(stateFile());
    if (!file.open(QIODevice::WriteOnly))
        return;
    file.write(QJsonDocument(s).toJson());
    if (file.commit())
        QFile::setPermissions(stateFile(), QFileDevice::ReadOwner | QFileDevice::WriteOwner);
}
