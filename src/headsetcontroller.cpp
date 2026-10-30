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
const QString kSteelSeriesVendor = QStringLiteral("0x1038");
const QString kTestVendor = QStringLiteral("0xf00b");
constexpr int kTimeoutMs = 20000;
constexpr int kPollMs = 20000;
constexpr qint64 kMaxOutput = 1 << 20; // la respuesta normal ocupa unos pocos KiB
constexpr int kMaxBands = 32;

bool s_testDevice = false;

// Ajustes de un solo valor entero: clave interna, capacidad que lo habilita,
// opción de headsetcontrol, rango admitido y nombre para los avisos.
struct Setting {
    const char *key;
    const char *cap;
    const char *flag;
    int min;
    int max;
    const char *label;
};

constexpr Setting kSettings[] = {
    {"sidetone", "CAP_SIDETONE", "-s", 0, 128, "Sidetone"},
    {"lights", "CAP_LIGHTS", "-l", 0, 1, "Luces"},
    {"inactive_time", "CAP_INACTIVE_TIME", "-i", 0, 90, "Apagado automático"},
    {"voice_prompts", "CAP_VOICE_PROMPTS", "-v", 0, 1, "Avisos de voz"},
    {"rotate_to_mute", "CAP_ROTATE_TO_MUTE", "-r", 0, 1, "Silenciar al subir el micro"},
    {"mic_led", "CAP_MICROPHONE_MUTE_LED_BRIGHTNESS", "--microphone-mute-led-brightness", 0, 3, "LED de silencio"},
    {"mic_volume", "CAP_MICROPHONE_VOLUME", "--microphone-volume", 0, 128, "Volumen del micro"},
    {"volume_limiter", "CAP_VOLUME_LIMITER", "--volume-limiter", 0, 1, "Limitador de volumen"},
    {"bt_powered_on", "CAP_BT_WHEN_POWERED_ON", "--bt-when-powered-on", 0, 1, "Bluetooth al encender"},
    {"bt_call_volume", "CAP_BT_CALL_VOLUME", "--bt-call-volume", 0, 100, "Volumen de llamadas"},
    {"noise_filter", "CAP_NOISE_FILTER", "--noise-filter", 0, 2, "Filtro de ruido"},
};

const Setting *findSetting(const QString &key)
{
    for (const Setting &s : kSettings)
        if (key == QLatin1String(s.key))
            return &s;
    return nullptr;
}

QString settingMessage(const Setting &s, int value)
{
    const QString label = QString::fromUtf8(s.label);
    if (s.max == 1)
        return QStringLiteral("%1 · %2").arg(label, value ? QStringLiteral("activado") : QStringLiteral("desactivado"));
    if (qstrcmp(s.key, "inactive_time") == 0)
        return value == 0 ? QStringLiteral("%1 · nunca").arg(label)
                          : QStringLiteral("%1 · %2 min").arg(label).arg(value);
    return QStringLiteral("%1 · %2").arg(label).arg(value);
}

QString configDir(const char *app)
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
        + QLatin1Char('/') + QLatin1String(app);
}

QString stateFile()
{
    return configDir("arctis-control") + QStringLiteral("/state.json");
}

// Archivo de la versión anterior (nova5-panel), solo para Arctis Nova 5.
QString legacyStateFile()
{
    return configDir("nova5-panel") + QStringLiteral("/state.json");
}

QJsonObject readJsonFile(const QString &path)
{
    // Solo un archivo normal: un FIFO bloquearía la apertura.
    const QFileInfo info(path);
    if (!info.isFile() || info.isSymLink())
        return {};
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(file.read(256 * 1024)).object();
}

// "0x1038" + "0x2232" → "1038:2232"
QString deviceKey(const QJsonObject &dev)
{
    auto hex = [&](const char *field) {
        QString v = dev.value(QLatin1String(field)).toString().toLower();
        if (v.startsWith(QLatin1String("0x")))
            v.remove(0, 2);
        return v.rightJustified(4, QLatin1Char('0'));
    };
    return hex("id_vendor") + QLatin1Char(':') + hex("id_product");
}

// Los presets llegan como objeto JSON y QJsonObject ordena las claves
// alfabéticamente, pero «-p N» usa el orden original. Lo sacamos del texto:
// el bloque «equalizer_presets» que sigue al «id_product» número `index`.
QStringList presetOrder(const QString &text, int index)
{
    qsizetype pos = -1;
    for (int i = 0; i <= index; ++i) {
        pos = text.indexOf(QStringLiteral("\"id_product\""), pos + 1);
        if (pos < 0)
            return {};
    }
    const qsizetype next = text.indexOf(QStringLiteral("\"id_product\""), pos + 1);
    const qsizetype start = text.indexOf(QStringLiteral("\"equalizer_presets\""), pos);
    if (start < 0 || (next >= 0 && start > next))
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

void HeadsetController::setTestDevice(bool enabled)
{
    s_testDevice = enabled;
}

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

// ---- Dispositivo seleccionado -------------------------------------------------

const HeadsetController::Device *HeadsetController::device() const
{
    return m_current >= 0 && m_current < m_devices.size() ? &m_devices.at(m_current) : nullptr;
}

// Toda orden lleva «-d»: sin él, headsetcontrol la aplica a todos los auriculares
// conectados (y en modo de prueba, también a los reales además del simulado).
QStringList HeadsetController::deviceArgs() const
{
    const Device *d = device();
    if (!d)
        return {};
    QStringList args{QStringLiteral("-d"), d->selector};
    if (s_testDevice)
        args.prepend(QStringLiteral("--test-device"));
    return args;
}

QStringList HeadsetController::deviceNames() const
{
    QStringList names;
    for (const Device &d : m_devices)
        names << d.name;
    return names;
}

void HeadsetController::setCurrentDevice(int index)
{
    if (index == m_current || index < 0 || index >= m_devices.size())
        return;
    m_current = index;
    emit deviceChanged();
    emit stateChanged();
    emit statusChanged();
}

QString HeadsetController::deviceName() const
{
    const Device *d = device();
    return d ? d->name : QString();
}

QString HeadsetController::deviceId() const
{
    const Device *d = device();
    return d ? d->id : QString();
}

QStringList HeadsetController::capabilities() const
{
    const Device *d = device();
    return d ? d->caps : QStringList{};
}

int HeadsetController::batteryLevel() const
{
    const Device *d = device();
    if (!d)
        return -1;
    const QJsonObject battery = d->status.value(QStringLiteral("battery")).toObject();
    return qBound(-1, battery.value(QStringLiteral("level")).toInt(-1), 100);
}

QString HeadsetController::batteryStatus() const
{
    const Device *d = device();
    return d ? d->status.value(QStringLiteral("battery")).toObject().value(QStringLiteral("status")).toString()
             : QString();
}

int HeadsetController::batteryMinutes() const
{
    const Device *d = device();
    if (!d)
        return -1;
    const int minutes = d->status.value(QStringLiteral("battery")).toObject()
                            .value(QStringLiteral("time_to_empty_min")).toInt(-1);
    return qBound(-1, minutes, 100 * 60);
}

int HeadsetController::chatmix() const
{
    const Device *d = device();
    return d ? qBound(-1, d->status.value(QStringLiteral("chatmix")).toInt(-1), 128) : -1;
}

// ---- Ecualizador ----------------------------------------------------------------

QStringList HeadsetController::presetNames() const
{
    const Device *d = device();
    return d ? d->presetNames : QStringList{};
}

int HeadsetController::eqBands() const
{
    const Device *d = device();
    if (!d)
        return 0;
    const int bands = d->status.value(QStringLiteral("equalizer")).toObject().value(QStringLiteral("bands")).toInt();
    return qBound(0, bands, kMaxBands);
}

double HeadsetController::eqMin() const
{
    const Device *d = device();
    return d ? d->status.value(QStringLiteral("equalizer")).toObject().value(QStringLiteral("min")).toDouble(-10) : -10;
}

double HeadsetController::eqMax() const
{
    const Device *d = device();
    return d ? d->status.value(QStringLiteral("equalizer")).toObject().value(QStringLiteral("max")).toDouble(10) : 10;
}

double HeadsetController::eqStep() const
{
    const Device *d = device();
    const double step = d ? d->status.value(QStringLiteral("equalizer")).toObject().value(QStringLiteral("step")).toDouble(1) : 1;
    return step > 0 ? step : 1;
}

int HeadsetController::eqPreset() const
{
    const Device *d = device();
    if (!d)
        return -1;
    const int preset = m_state.value(d->id).value(QStringLiteral("eq_preset")).toInt(-1);
    return preset < d->presetNames.size() ? preset : -1;
}

QList<double> HeadsetController::activeBands() const
{
    const Device *d = device();
    if (!d)
        return {};
    const int preset = eqPreset();
    if (preset >= 0 && preset < d->presetBands.size())
        return d->presetBands.at(preset);
    const QJsonArray custom = m_state.value(d->id).value(QStringLiteral("eq_custom")).toArray();
    QList<double> bands(eqBands(), 0.0);
    if (custom.size() == bands.size())
        for (qsizetype i = 0; i < bands.size(); ++i)
            bands[i] = qBound(eqMin(), custom.at(i).toDouble(), eqMax());
    return bands;
}

// ---- Órdenes ------------------------------------------------------------------

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

void HeadsetController::refresh()
{
    // Evita amontonar lecturas si el auricular tarda en responder.
    if (m_job.isRefresh && m_running)
        return;
    for (const Job &job : m_queue)
        if (job.isRefresh)
            return;
    QStringList args;
    if (s_testDevice)
        args << QStringLiteral("--test-device");
    enqueue({args, nullptr, {}, true});
}

int HeadsetController::setting(const QString &key) const
{
    const Device *d = device();
    const Setting *s = findSetting(key);
    if (!d || !s)
        return -1;
    // Algunos auriculares sí informan del sidetone actual.
    if (key == QLatin1String("sidetone")) {
        const QJsonValue level = d->status.value(QStringLiteral("sidetone")).toObject().value(QStringLiteral("level"));
        if (level.isDouble())
            return qBound(s->min, level.toInt(), s->max);
    }
    const QJsonValue v = m_state.value(d->id).value(key);
    return v.isDouble() ? qBound(s->min, v.toInt(), s->max) : -1;
}

void HeadsetController::apply(const QString &key, int value)
{
    const Device *d = device();
    const Setting *s = findSetting(key);
    if (!d || !s)
        return emit notify(tr("Ajuste no válido"), true);
    if (!d->caps.contains(QLatin1String(s->cap)))
        return emit notify(tr("Este auricular no admite ese ajuste"), true);
    if (value < s->min || value > s->max)
        return emit notify(tr("Valor fuera de rango (%1 a %2)").arg(s->min).arg(s->max), true);
    enqueue({deviceArgs() + QStringList{QString::fromLatin1(s->flag), QString::number(value)},
             [key, value](QJsonObject &state) { state.insert(key, value); },
             settingMessage(*s, value)});
}

void HeadsetController::setEqPreset(int index)
{
    const Device *d = device();
    if (!d || !d->caps.contains(QLatin1String("CAP_EQUALIZER_PRESET")))
        return emit notify(tr("Este auricular no tiene presets"), true);
    if (index < 0 || index >= d->presetNames.size())
        return emit notify(tr("Preset no válido"), true);
    enqueue({deviceArgs() + QStringList{QStringLiteral("-p"), QString::number(index)},
             [index](QJsonObject &state) { state.insert(QStringLiteral("eq_preset"), index); },
             tr("Preset «%1» aplicado").arg(d->presetNames.at(index))});
}

void HeadsetController::setEqCustom(const QList<double> &bands)
{
    const Device *d = device();
    if (!d || !d->caps.contains(QLatin1String("CAP_EQUALIZER")))
        return emit notify(tr("Este auricular no tiene ecualizador"), true);
    if (bands.size() != eqBands())
        return emit notify(tr("El ecualizador necesita %1 bandas").arg(eqBands()), true);
    const double lo = eqMin(), hi = eqMax(), step = eqStep();
    QJsonArray stored;
    QStringList parts;
    for (const double v : bands) {
        if (!std::isfinite(v) || v < lo || v > hi)
            return emit notify(tr("Valor fuera de rango (%1 a %2)").arg(lo).arg(hi), true);
        const double rounded = qBound(lo, std::round(v / step) * step, hi);
        stored << rounded;
        parts << QString::number(rounded, 'g', 4);
    }
    enqueue({deviceArgs() + QStringList{QStringLiteral("-e"), parts.join(QLatin1Char(','))},
             [stored](QJsonObject &state) {
                 state.insert(QStringLiteral("eq_custom"), stored);
                 state.remove(QStringLiteral("eq_preset"));
             },
             tr("Ecualizador aplicado")});
}

void HeadsetController::playNotification()
{
    const Device *d = device();
    if (!d || !d->caps.contains(QLatin1String("CAP_NOTIFICATION_SOUND")))
        return;
    enqueue({deviceArgs() + QStringList{QStringLiteral("-n"), QStringLiteral("0")}, nullptr,
             tr("Sonido de aviso enviado")});
}

void HeadsetController::enqueue(Job job)
{
    if (!job.isRefresh && !job.args.contains(QStringLiteral("-d")))
        return emit notify(tr("No hay ningún auricular seleccionado"), true);
    m_queue.enqueue(std::move(job));
    emit busyChanged();
    if (!m_running)
        runNext();
}

void HeadsetController::runNext()
{
    if (m_queue.isEmpty()) {
        m_running = false;
        m_job = {};
        emit busyChanged();
        return;
    }
    m_job = m_queue.dequeue();
    m_running = true;
    m_process.setArguments(m_job.args + QStringList{QStringLiteral("-o"), QStringLiteral("json")});
    m_killTimer.start();
    m_process.start(QIODevice::ReadOnly);
}

void HeadsetController::handleFinished(const QByteArray &output, const QString &failure)
{
    const Job job = std::exchange(m_job, Job{});
    const QJsonObject root = QJsonDocument::fromJson(output).object();
    const QJsonArray devices = root.value(QStringLiteral("devices")).toArray();

    if (job.isRefresh) {
        if (output.isEmpty() && !failure.isEmpty())
            setNoDevice(failure);
        else
            applyDevices(devices, output);
        return runNext();
    }

    // El resultado de cada orden llega en «actions». Sin acciones, la orden no
    // se ejecutó (p. ej. headsetcontrol no la entendió): nunca se da por buena.
    const QJsonArray actions = root.value(QStringLiteral("actions")).toArray();
    QStringList errors;
    for (const QJsonValue &v : actions) {
        const QJsonObject action = v.toObject();
        if (action.value(QStringLiteral("status")).toString() != QLatin1String("success"))
            errors << action.value(QStringLiteral("error_message")).toString(tr("La orden falló"));
    }
    if (actions.isEmpty())
        errors << (!failure.isEmpty() ? failure.section(QLatin1Char('\n'), 0, 0) : tr("El auricular no respondió"));

    if (!errors.isEmpty()) {
        emit notify(errors.join(QStringLiteral("; ")), true);
        refresh();
    } else {
        if (job.onSuccess && device()) {
            job.onSuccess(currentState());
            saveState();
            emit stateChanged();
        }
        emit notify(job.successMessage, false);
    }
    runNext();
}

void HeadsetController::applyDevices(const QJsonArray &devices, const QByteArray &rawOutput)
{
    const QString raw = QString::fromUtf8(rawOutput);
    const QString previousId = deviceId();
    const QStringList previousNames = deviceNames();
    const QStringList previousCaps = capabilities();
    const QStringList previousPresets = presetNames();

    QList<Device> found;
    for (qsizetype i = 0; i < devices.size(); ++i) {
        const QJsonObject dev = devices.at(i).toObject();
        // Solo SteelSeries; en modo de prueba, solo el simulado (las órdenes van a él).
        const QString vendor = dev.value(QStringLiteral("id_vendor")).toString().toLower();
        if (vendor != (s_testDevice ? kTestVendor : kSteelSeriesVendor))
            continue;
        if (dev.value(QStringLiteral("status")).toString() == QLatin1String("error"))
            continue;

        Device d;
        d.id = deviceKey(dev);
        d.selector = QStringLiteral("0x") + d.id.left(4) + QStringLiteral(":0x") + d.id.mid(5);
        d.name = dev.value(QStringLiteral("device")).toString();
        if (d.name.startsWith(QLatin1String("SteelSeries ")))
            d.name.remove(0, 12);
        for (const QJsonValue &cap : dev.value(QStringLiteral("capabilities")).toArray())
            d.caps << cap.toString();
        d.status = dev;

        const QJsonObject presets = dev.value(QStringLiteral("equalizer_presets")).toObject();
        for (const QString &name : presetOrder(raw, int(i))) {
            const QJsonArray values = presets.value(name).toArray();
            if (values.isEmpty() || values.size() > kMaxBands)
                continue;
            QList<double> bands;
            for (const QJsonValue &v : values)
                bands << v.toDouble();
            d.presetNames << name;
            d.presetBands << bands;
        }
        found << d;
    }

    if (found.isEmpty())
        return setNoDevice(tr("No se detecta ningún auricular SteelSeries compatible"));

    m_devices = found;
    m_errorText.clear();
    // Se mantiene el auricular elegido si sigue conectado.
    m_current = 0;
    for (qsizetype i = 0; i < m_devices.size(); ++i)
        if (m_devices.at(i).id == previousId)
            m_current = int(i);
    m_updatedText = QTime::currentTime().toString(QStringLiteral("HH:mm"));

    if (deviceNames() != previousNames)
        emit devicesChanged();
    if (deviceId() != previousId || capabilities() != previousCaps || presetNames() != previousPresets) {
        emit deviceChanged();
        emit stateChanged();
    }
    emit statusChanged();
}

void HeadsetController::setNoDevice(const QString &reason)
{
    if (m_devices.isEmpty() && m_errorText == reason)
        return;
    const bool hadDevices = !m_devices.isEmpty();
    m_devices.clear();
    m_current = -1;
    m_errorText = reason;
    if (hadDevices) {
        emit devicesChanged();
        emit deviceChanged();
        emit stateChanged();
    }
    emit statusChanged();
}

// ---- Estado guardado -------------------------------------------------------------

QJsonObject &HeadsetController::currentState()
{
    return m_state[deviceId()];
}

void HeadsetController::loadState()
{
    static const QRegularExpression idPattern(QStringLiteral("^[0-9a-f]{4}:[0-9a-f]{4}$"));

    const QJsonObject root = readJsonFile(stateFile());
    const QJsonObject saved = root.value(QStringLiteral("devices")).toObject();
    for (auto it = saved.begin(); it != saved.end() && m_state.size() < 64; ++it) {
        if (!idPattern.match(it.key()).hasMatch())
            continue;
        const QJsonObject in = it.value().toObject();
        QJsonObject out;
        // Solo claves conocidas y dentro de rango.
        for (const Setting &s : kSettings) {
            const QJsonValue v = in.value(QLatin1String(s.key));
            if (v.isDouble())
                out.insert(QLatin1String(s.key), qBound(s.min, v.toInt(), s.max));
        }
        const QJsonValue preset = in.value(QStringLiteral("eq_preset"));
        if (preset.isDouble())
            out.insert(QStringLiteral("eq_preset"), qBound(0, preset.toInt(), 31));
        const QJsonArray custom = in.value(QStringLiteral("eq_custom")).toArray();
        if (!custom.isEmpty() && custom.size() <= kMaxBands) {
            QJsonArray bands;
            for (const QJsonValue &v : custom)
                bands << qBound(-30.0, v.toDouble(), 30.0);
            out.insert(QStringLiteral("eq_custom"), bands);
        }
        m_state.insert(it.key(), out);
    }
    if (!root.isEmpty())
        return;

    // Primera vez: se recuperan los ajustes de nova5-panel (Arctis Nova 5).
    const QJsonObject legacy = readJsonFile(legacyStateFile());
    if (legacy.isEmpty())
        return;
    QJsonObject nova5;
    if (legacy.value(QStringLiteral("eq_preset")).isDouble())
        nova5.insert(QStringLiteral("eq_preset"), qBound(0, legacy.value(QStringLiteral("eq_preset")).toInt(), 3));
    const QJsonArray custom = legacy.value(QStringLiteral("eq_custom")).toArray();
    if (custom.size() == 10) {
        QJsonArray bands;
        for (const QJsonValue &v : custom)
            bands << qBound(-10.0, v.toDouble(), 10.0);
        nova5.insert(QStringLiteral("eq_custom"), bands);
    }
    if (legacy.value(QStringLiteral("volume_limiter")).isBool())
        nova5.insert(QStringLiteral("volume_limiter"), legacy.value(QStringLiteral("volume_limiter")).toBool() ? 1 : 0);
    if (legacy.value(QStringLiteral("inactive_time")).isDouble())
        nova5.insert(QStringLiteral("inactive_time"), qBound(0, legacy.value(QStringLiteral("inactive_time")).toInt(), 90));
    m_state.insert(QStringLiteral("1038:2232"), nova5);
    saveState();
}

void HeadsetController::saveState() const
{
    if (s_testDevice)
        return; // el dispositivo simulado no toca los ajustes reales
    QJsonObject devices;
    for (auto it = m_state.cbegin(); it != m_state.cend(); ++it)
        if (!it.key().isEmpty() && !it.value().isEmpty())
            devices.insert(it.key(), it.value());
    const QJsonObject root{{QStringLiteral("version"), 2}, {QStringLiteral("devices"), devices}};

    const QString dir = configDir("arctis-control");
    QDir().mkpath(dir);
    QFile::setPermissions(dir, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    QSaveFile file(stateFile());
    if (!file.open(QIODevice::WriteOnly))
        return;
    file.write(QJsonDocument(root).toJson());
    if (file.commit())
        QFile::setPermissions(stateFile(), QFileDevice::ReadOwner | QFileDevice::WriteOwner);
}
