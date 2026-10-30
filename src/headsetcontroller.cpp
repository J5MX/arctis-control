#include "headsetcontroller.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTime>

#include <algorithm>
#include <cmath>
#include <utility>

namespace {

// Distribution packages install it in /usr/bin; building it from source (needed on
// Debian and some Ubuntu releases) puts it in /usr/local/bin. Both are root-owned.
QString headsetControlPath()
{
    const QString configured = QStringLiteral(HEADSETCONTROL_PATH);
    const QString local = QStringLiteral("/usr/local/bin/headsetcontrol");
    if (configured == QLatin1String("/usr/bin/headsetcontrol") && !QFileInfo(configured).isExecutable()
        && QFileInfo(local).isExecutable())
        return local;
    return configured;
}
const QString kSteelSeriesVendor = QStringLiteral("0x1038");
const QString kTestVendor = QStringLiteral("0xf00b");
constexpr int kTimeoutMs = 20000;
constexpr int kPollMs = 20000;
constexpr int kBackgroundPollMs = 60000;
constexpr qint64 kMaxOutput = 1 << 20; // a normal reply is a few KiB
constexpr int kMaxBands = 32;
// Upper bounds for data coming from headsetcontrol, so odd output cannot
// blow up memory or the UI.
constexpr int kMaxDevices = 16;
constexpr int kMaxCaps = 64;
constexpr int kMaxPresets = 32;
constexpr int kMaxNameLength = 80;

bool s_testDevice = false;

// Single-integer settings: internal key, capability that enables it,
// headsetcontrol option, accepted range and the label used in notifications.
struct Setting {
    const char *key;
    const char *cap;
    const char *flag;
    int min;
    int max;
    const char *label;
};

constexpr Setting kSettings[] = {
    {"sidetone", "CAP_SIDETONE", "-s", 0, 128, QT_TRANSLATE_NOOP("Setting", "Sidetone")},
    {"lights", "CAP_LIGHTS", "-l", 0, 1, QT_TRANSLATE_NOOP("Setting", "Lights")},
    {"inactive_time", "CAP_INACTIVE_TIME", "-i", 0, 90, QT_TRANSLATE_NOOP("Setting", "Auto power-off")},
    {"voice_prompts", "CAP_VOICE_PROMPTS", "-v", 0, 1, QT_TRANSLATE_NOOP("Setting", "Voice prompts")},
    {"rotate_to_mute", "CAP_ROTATE_TO_MUTE", "-r", 0, 1, QT_TRANSLATE_NOOP("Setting", "Rotate to mute")},
    {"mic_led", "CAP_MICROPHONE_MUTE_LED_BRIGHTNESS", "--microphone-mute-led-brightness", 0, 3, QT_TRANSLATE_NOOP("Setting", "Mute LED")},
    {"mic_volume", "CAP_MICROPHONE_VOLUME", "--microphone-volume", 0, 128, QT_TRANSLATE_NOOP("Setting", "Mic volume")},
    {"volume_limiter", "CAP_VOLUME_LIMITER", "--volume-limiter", 0, 1, QT_TRANSLATE_NOOP("Setting", "Volume limiter")},
    {"bt_powered_on", "CAP_BT_WHEN_POWERED_ON", "--bt-when-powered-on", 0, 1, QT_TRANSLATE_NOOP("Setting", "Bluetooth on power-up")},
    {"bt_call_volume", "CAP_BT_CALL_VOLUME", "--bt-call-volume", 0, 100, QT_TRANSLATE_NOOP("Setting", "Call volume")},
    {"noise_filter", "CAP_NOISE_FILTER", "--noise-filter", 0, 2, QT_TRANSLATE_NOOP("Setting", "Noise filter")},
};

// Integer from JSON clamped to [lo, hi]. Clamps the real value: QJsonValue::toInt()
// returns 0 for numbers that do not fit an int, which would turn 1e12 into 0.
int jsonInt(const QJsonValue &value, int lo, int hi, int fallback)
{
    if (!value.isDouble() || !std::isfinite(value.toDouble()))
        return fallback;
    return int(qBound(double(lo), std::round(value.toDouble()), double(hi)));
}

const Setting *findSetting(const QString &key)
{
    for (const Setting &s : kSettings)
        if (key == QLatin1String(s.key))
            return &s;
    return nullptr;
}

QString settingMessage(const Setting &s, int value)
{
    const QString label = QCoreApplication::translate("Setting", s.label);
    if (s.max == 1)
        return QStringLiteral("%1 · %2").arg(label, value ? QCoreApplication::translate("Setting", "on")
                                                          : QCoreApplication::translate("Setting", "off"));
    if (qstrcmp(s.key, "inactive_time") == 0)
        return value == 0 ? QStringLiteral("%1 · %2").arg(label, QCoreApplication::translate("Setting", "never"))
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

// State file of the previous app (nova5-panel), which only supported the Arctis Nova 5.
QString legacyStateFile()
{
    return configDir("nova5-panel") + QStringLiteral("/state.json");
}

QJsonObject readJsonFile(const QString &path)
{
    // Regular files only (a FIFO would block). Symlinks to regular files are fine,
    // so dotfile managers such as GNU Stow keep working.
    const QFileInfo info(path);
    if (!info.isFile())
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

// Presets arrive as a JSON object and QJsonObject sorts its keys alphabetically,
// but "-p N" uses the original order, so it is read from the raw text: the
// "equalizer_presets" block that follows the `index`-th "id_product".
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
    // Minimal environment: headsetcontrol needs nothing from ours.
    QProcessEnvironment env;
    env.insert(QStringLiteral("LANG"), QStringLiteral("C.UTF-8"));
    m_process.setProcessEnvironment(env);
    m_process.setProgram(headsetControlPath());

    m_killTimer.setSingleShot(true);
    m_killTimer.setInterval(kTimeoutMs);
    connect(&m_killTimer, &QTimer::timeout, this, [this] { m_process.kill(); });

    connect(&m_process, &QProcess::finished, this, [this](int, QProcess::ExitStatus status) {
        m_killTimer.stop();
        const QByteArray out = m_process.read(kMaxOutput);
        const QString err = QString::fromUtf8(m_process.readAllStandardError().left(4096)).trimmed();
        if (m_process.bytesAvailable() > 0) {
            m_process.readAll();
            return handleFinished({}, tr("headsetcontrol output is too large"));
        }
        handleFinished(out, status == QProcess::CrashExit ? tr("headsetcontrol did not respond") : err);
    });
    // Stop as soon as the output exceeds the limit instead of buffering it.
    connect(&m_process, &QProcess::readyReadStandardOutput, this, [this] {
        if (m_process.bytesAvailable() > kMaxOutput)
            m_process.kill();
    });
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            m_killTimer.stop();
            handleFinished({}, tr("headsetcontrol is not installed"));
        }
    });

    m_pollTimer.setInterval(kPollMs);
    connect(&m_pollTimer, &QTimer::timeout, this, &HeadsetController::refresh);

    loadState();
    // First of all: find out whether this headsetcontrol can target one device.
    enqueue({{QStringLiteral("-h")}, nullptr, {}, false, {}, true});
    setPolling(true);
}

HeadsetController::~HeadsetController()
{
    // Stop a running headsetcontrol before the members go away: otherwise the
    // process destructor emits finished() into handlers that use destroyed timers.
    m_process.disconnect(this);
    if (m_process.state() != QProcess::NotRunning) {
        m_process.kill();
        m_process.waitForFinished(1000);
    }
}

// ---- Selected device -----------------------------------------------------------

const HeadsetController::Device *HeadsetController::device() const
{
    return m_current >= 0 && m_current < m_devices.size() ? &m_devices.at(m_current) : nullptr;
}

// Every command carries "-d": without it headsetcontrol applies it to every
// connected headset (and in test mode, to the real ones besides the mock).
// Old versions without "-d" only get a command when they list a single device.
QStringList HeadsetController::deviceArgs() const
{
    const Device *d = device();
    if (!d)
        return {};
    QStringList args;
    if (m_selection == DeviceSelection::Supported)
        args << QStringLiteral("-d") << d->selector;
    if (s_testDevice)
        args.prepend(QStringLiteral("--test-device"));
    return args;
}

bool HeadsetController::canTarget() const
{
    if (!device())
        return false;
    return m_selection == DeviceSelection::Supported
        || (m_selection == DeviceSelection::Unsupported && m_reportedDevices == 1);
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
    return jsonInt(battery.value(QStringLiteral("level")), -1, 100, -1); // -1: off or unknown
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
    return jsonInt(d->status.value(QStringLiteral("battery")).toObject().value(QStringLiteral("time_to_empty_min")),
                   -1, 100 * 60, -1);
}

int HeadsetController::chatmix() const
{
    const Device *d = device();
    return d ? jsonInt(d->status.value(QStringLiteral("chatmix")), -1, 128, -1) : -1;
}

// ---- Equalizer -----------------------------------------------------------------

QStringList HeadsetController::presetNames() const
{
    const Device *d = device();
    return d ? d->presetNames : QStringList{};
}

int HeadsetController::eqBands() const
{
    const Device *d = device();
    return d ? d->eqBands : 0;
}

double HeadsetController::eqMin() const
{
    const Device *d = device();
    return d ? d->eqMin : -10;
}

double HeadsetController::eqMax() const
{
    const Device *d = device();
    return d ? d->eqMax : 10;
}

double HeadsetController::eqStep() const
{
    const Device *d = device();
    return d ? d->eqStep : 1;
}

int HeadsetController::eqPreset() const
{
    const Device *d = device();
    if (!d)
        return -1;
    const int preset = jsonInt(m_state.value(d->id).value(QStringLiteral("eq_preset")), 0, kMaxPresets - 1, -1);
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
    QList<double> bands(d->eqBands, qBound(d->eqMin, 0.0, d->eqMax));
    if (custom.size() == bands.size())
        for (qsizetype i = 0; i < bands.size(); ++i)
            bands[i] = qBound(d->eqMin, custom.at(i).toDouble(), d->eqMax);
    return bands;
}

// ---- Commands ------------------------------------------------------------------

void HeadsetController::setPolling(bool enabled)
{
    if (enabled == m_pollTimer.isActive())
        return;
    if (enabled) {
        m_pollTimer.start();
        refresh(); // fresh data as soon as the window is back
    } else {
        m_pollTimer.stop();
    }
    emit pollingChanged();
}

void HeadsetController::setBackground(bool background)
{
    if (background == m_background)
        return;
    m_background = background;
    m_pollTimer.setInterval(background ? kBackgroundPollMs : kPollMs);
    if (!background)
        refresh(); // fresh data as soon as the window is shown
    emit backgroundChanged();
}

void HeadsetController::refresh()
{
    // Don't pile up reads if the headset is slow to answer.
    if (m_job.isRefresh && m_running)
        return;
    for (const Job &job : m_queue)
        if (job.isRefresh)
            return;
    QStringList args;
    if (s_testDevice)
        args << QStringLiteral("--test-device");
    enqueue({args, nullptr, {}, true, {}});
}

int HeadsetController::setting(const QString &key) const
{
    const Device *d = device();
    const Setting *s = findSetting(key);
    if (!d || !s)
        return -1;
    // Some headsets do report their current sidetone.
    if (key == QLatin1String("sidetone")) {
        const QJsonValue level = d->status.value(QStringLiteral("sidetone")).toObject().value(QStringLiteral("level"));
        if (level.isDouble())
            return jsonInt(level, s->min, s->max, -1);
    }
    const QJsonValue v = m_state.value(d->id).value(key);
    return jsonInt(v, s->min, s->max, -1);
}

bool HeadsetController::supports(const QString &key) const
{
    const Device *d = device();
    if (!d)
        return false;
    if (key == QLatin1String("eq_preset"))
        return d->caps.contains(QLatin1String("CAP_EQUALIZER_PRESET")) && !d->presetNames.isEmpty();
    if (key == QLatin1String("eq_custom"))
        return d->caps.contains(QLatin1String("CAP_EQUALIZER")) && d->eqBands > 0;
    if (key == QLatin1String("notification"))
        return d->caps.contains(QLatin1String("CAP_NOTIFICATION_SOUND"));
    const Setting *s = findSetting(key);
    return s && d->caps.contains(QLatin1String(s->cap));
}

void HeadsetController::apply(const QString &key, int value)
{
    const Device *d = device();
    const Setting *s = findSetting(key);
    if (!d || !s)
        return emit notify(tr("Invalid setting"), true);
    if (!d->caps.contains(QLatin1String(s->cap)))
        return emit notify(tr("This headset does not support that setting"), true);
    if (value < s->min || value > s->max)
        return emit notify(tr("Value out of range (%1 to %2)").arg(s->min).arg(s->max), true);
    enqueue({deviceArgs() + QStringList{QString::fromLatin1(s->flag), QString::number(value)},
             [key, value](QJsonObject &state) { state.insert(key, value); },
             settingMessage(*s, value), false, d->id});
}

void HeadsetController::setEqPreset(int index)
{
    const Device *d = device();
    if (!d || !d->caps.contains(QLatin1String("CAP_EQUALIZER_PRESET")))
        return emit notify(tr("This headset has no presets"), true);
    if (index < 0 || index >= d->presetNames.size())
        return emit notify(tr("Invalid preset"), true);
    enqueue({deviceArgs() + QStringList{QStringLiteral("-p"), QString::number(index)},
             [index](QJsonObject &state) { state.insert(QStringLiteral("eq_preset"), index); },
             tr("Preset “%1” applied").arg(d->presetNames.at(index)), false, d->id});
}

void HeadsetController::setEqCustom(const QList<double> &bands)
{
    const Device *d = device();
    if (!d || !d->caps.contains(QLatin1String("CAP_EQUALIZER")))
        return emit notify(tr("This headset has no equalizer"), true);
    if (bands.size() != d->eqBands)
        return emit notify(tr("The equalizer needs %1 bands").arg(d->eqBands), true);
    const double lo = d->eqMin, hi = d->eqMax, step = d->eqStep;
    QJsonArray stored;
    QStringList parts;
    for (const double v : bands) {
        if (!std::isfinite(v) || v < lo || v > hi)
            return emit notify(tr("Value out of range (%1 to %2)").arg(lo).arg(hi), true);
        const double rounded = qBound(lo, std::round(v / step) * step, hi);
        stored << rounded;
        parts << QString::number(rounded, 'g', 4);
    }
    enqueue({deviceArgs() + QStringList{QStringLiteral("-e"), parts.join(QLatin1Char(','))},
             [stored](QJsonObject &state) {
                 state.insert(QStringLiteral("eq_custom"), stored);
                 state.remove(QStringLiteral("eq_preset"));
             },
             tr("Equalizer applied"), false, d->id});
}

void HeadsetController::playNotification()
{
    const Device *d = device();
    if (!d || !d->caps.contains(QLatin1String("CAP_NOTIFICATION_SOUND")))
        return emit notify(tr("This headset does not support that setting"), true);
    enqueue({deviceArgs() + QStringList{QStringLiteral("-n"), QStringLiteral("0")}, nullptr,
             tr("Notification sound sent"), false, {}});
}

void HeadsetController::enqueue(Job job)
{
    if (!job.isRefresh && !job.isProbe && !canTarget()) {
        if (!device())
            return emit notify(tr("No headset selected"), true);
        return emit notify(tr("Several headsets are connected and this HeadsetControl version cannot pick one. "
                              "Update HeadsetControl to 4.0 or newer."), true);
    }
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
    m_process.setArguments(m_job.isProbe ? m_job.args
                                         : m_job.args + QStringList{QStringLiteral("-o"), QStringLiteral("json")});
    m_killTimer.start();
    m_process.start(QIODevice::ReadOnly);
}

void HeadsetController::handleFinished(const QByteArray &output, const QString &failure)
{
    const Job job = std::exchange(m_job, Job{});
    if (job.isProbe) {
        m_selection = output.contains("--device") ? DeviceSelection::Supported : DeviceSelection::Unsupported;
        return runNext();
    }
    const QJsonObject root = QJsonDocument::fromJson(output).object();
    const QJsonArray devices = root.value(QStringLiteral("devices")).toArray();

    if (job.isRefresh) {
        m_reportedDevices = int(devices.size());
        if (output.isEmpty() && !failure.isEmpty())
            setNoDevice(failure);
        else
            applyDevices(devices, output);
        return runNext();
    }

    // The result of each command comes in "actions". No actions means the command
    // never ran (e.g. headsetcontrol rejected it): never treat that as success.
    const QJsonArray actions = root.value(QStringLiteral("actions")).toArray();
    QStringList errors;
    for (const QJsonValue &v : actions) {
        const QJsonObject action = v.toObject();
        if (action.value(QStringLiteral("status")).toString() != QLatin1String("success"))
            errors << action.value(QStringLiteral("error_message")).toString(tr("The command failed"));
    }
    if (actions.isEmpty())
        errors << (!failure.isEmpty() ? failure.section(QLatin1Char('\n'), 0, 0) : tr("The headset did not respond"));

    if (!errors.isEmpty()) {
        emit notify(errors.join(QStringLiteral("; ")), true);
        refresh();
    } else {
        if (job.onSuccess && !job.deviceId.isEmpty()) {
            job.onSuccess(m_state[job.deviceId]);
            saveState();
            if (job.deviceId == deviceId())
                emit stateChanged();
        }
        emit notify(job.successMessage, false);
    }
    runNext();
}

void HeadsetController::applyDevices(const QJsonArray &devices, const QByteArray &rawOutput)
{
    const QString previousId = deviceId();
    const QStringList previousNames = deviceNames();
    const QStringList previousCaps = capabilities();
    const QStringList previousPresets = presetNames();
    const QJsonObject previousStatus = device() ? device()->status : QJsonObject{};
    const QString previousUpdated = m_updatedText;
    const bool wasConnected = connected();

    QString raw; // decoded only if some device's presets need parsing
    QList<Device> found;
    for (qsizetype i = 0; i < devices.size() && found.size() < kMaxDevices; ++i) {
        const QJsonObject dev = devices.at(i).toObject();
        // SteelSeries only; in test mode only the mock (commands go to it).
        const QString vendor = dev.value(QStringLiteral("id_vendor")).toString().toLower();
        if (vendor != (s_testDevice ? kTestVendor : kSteelSeriesVendor))
            continue;
        if (dev.value(QStringLiteral("status")).toString() == QLatin1String("error"))
            continue;

        Device d;
        d.id = deviceKey(dev);
        if (d.id.size() != 9)
            continue;
        d.selector = QStringLiteral("0x") + d.id.left(4) + QStringLiteral(":0x") + d.id.mid(5);
        d.name = dev.value(QStringLiteral("device")).toString().left(kMaxNameLength);
        if (d.name.startsWith(QLatin1String("SteelSeries ")))
            d.name.remove(0, 12);
        for (const QJsonValue &cap : dev.value(QStringLiteral("capabilities")).toArray()) {
            if (d.caps.size() >= kMaxCaps)
                break;
            d.caps << cap.toString().left(64);
        }
        d.status = dev;

        // Equalizer geometry: sane limits even if the reported values are odd.
        const QJsonObject eq = dev.value(QStringLiteral("equalizer")).toObject();
        d.eqBands = jsonInt(eq.value(QStringLiteral("bands")), 0, kMaxBands, 0);
        d.eqMin = qBound(-100.0, eq.value(QStringLiteral("min")).toDouble(-10), 0.0);
        d.eqMax = qBound(0.0, eq.value(QStringLiteral("max")).toDouble(10), 100.0);
        d.eqStep = qBound(0.01, eq.value(QStringLiteral("step")).toDouble(1), 10.0);
        if (d.eqMax - d.eqMin < d.eqStep)
            d.eqBands = 0; // unusable range: hide the bands

        // Presets only need parsing when they change (normally once per device).
        const QJsonValue presets = dev.value(QStringLiteral("equalizer_presets"));
        const auto previous = std::find_if(m_devices.cbegin(), m_devices.cend(),
                                           [&](const Device &p) { return p.id == d.id; });
        if (previous != m_devices.cend()
            && previous->status.value(QStringLiteral("equalizer_presets")) == presets) {
            d.presetNames = previous->presetNames;
            d.presetBands = previous->presetBands;
        } else if (presets.isObject()) {
            if (raw.isEmpty())
                raw = QString::fromUtf8(rawOutput);
            const QJsonObject presetObject = presets.toObject();
            for (const QString &name : presetOrder(raw, int(i))) {
                if (d.presetNames.size() >= kMaxPresets)
                    break;
                const QJsonArray values = presetObject.value(name).toArray();
                if (values.isEmpty() || values.size() > kMaxBands)
                    continue;
                QList<double> bands;
                for (const QJsonValue &v : values)
                    bands << qBound(-100.0, v.toDouble(), 100.0);
                d.presetNames << name.left(kMaxNameLength);
                d.presetBands << bands;
            }
        }
        found << d;
    }

    if (found.isEmpty())
        return setNoDevice(tr("No supported SteelSeries headset detected"));

    m_devices = found;
    m_errorText.clear();
    // Keep the selected headset if it is still connected.
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
    // Only wake the UI bindings when something visible actually changed.
    if (!wasConnected || deviceId() != previousId || device()->status != previousStatus
        || m_updatedText != previousUpdated)
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

// ---- Saved state ---------------------------------------------------------------

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
        // Known keys only, clamped to their ranges.
        for (const Setting &s : kSettings) {
            const QJsonValue v = in.value(QLatin1String(s.key));
            if (v.isDouble())
                out.insert(QLatin1String(s.key), jsonInt(v, s.min, s.max, s.min));
        }
        const QJsonValue preset = in.value(QStringLiteral("eq_preset"));
        if (preset.isDouble())
            out.insert(QStringLiteral("eq_preset"), jsonInt(preset, 0, kMaxPresets - 1, 0));
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

    // First run: import the settings of nova5-panel (Arctis Nova 5).
    const QJsonObject legacy = readJsonFile(legacyStateFile());
    if (legacy.isEmpty())
        return;
    QJsonObject nova5;
    if (legacy.value(QStringLiteral("eq_preset")).isDouble())
        nova5.insert(QStringLiteral("eq_preset"), jsonInt(legacy.value(QStringLiteral("eq_preset")), 0, 3, 0));
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
        nova5.insert(QStringLiteral("inactive_time"), jsonInt(legacy.value(QStringLiteral("inactive_time")), 0, 90, 5));
    m_state.insert(QStringLiteral("1038:2232"), nova5);
    saveState();
}

void HeadsetController::saveState() const
{
    if (s_testDevice)
        return; // the mock device never touches the real settings
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
