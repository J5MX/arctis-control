#include "headsetmic.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>

namespace {

const QString kPactl = QStringLiteral(PACTL_PATH);
constexpr int kTimeoutMs = 10000;
constexpr int kPollMs = 20000;
constexpr qint64 kMaxOutput = 4 << 20; // the card list is normally tens of KiB

// Card and profile names come from PipeWire and are passed back to pactl as
// arguments: reject anything that could be read as an option or is malformed.
bool safeName(const QString &name)
{
    if (name.isEmpty() || name.size() > 256 || name.startsWith(QLatin1Char('-')))
        return false;
    for (const QChar c : name)
        if (c.unicode() < 0x20 || c == QChar(0x7f))
            return false;
    return true;
}

// "output:analog-stereo+input:mono-fallback" → "output:analog-stereo"
QString outputPart(const QString &profile)
{
    if (!profile.startsWith(QLatin1String("output:")))
        return {};
    return profile.section(QStringLiteral("+input:"), 0, 0);
}

} // namespace

HeadsetMic::HeadsetMic(QObject *parent)
    : QObject(parent)
{
    // pactl only needs to find the PipeWire/PulseAudio socket and its cookie.
    const QProcessEnvironment system = QProcessEnvironment::systemEnvironment();
    QProcessEnvironment env;
    env.insert(QStringLiteral("LANG"), QStringLiteral("C.UTF-8"));
    for (const char *name : {"XDG_RUNTIME_DIR", "HOME", "PULSE_SERVER", "PULSE_COOKIE"}) {
        const QString key = QString::fromLatin1(name);
        if (system.contains(key))
            env.insert(key, system.value(key));
    }
    m_process.setProcessEnvironment(env);
    m_process.setProgram(kPactl);

    m_killTimer.setSingleShot(true);
    m_killTimer.setInterval(kTimeoutMs);
    connect(&m_killTimer, &QTimer::timeout, this, [this] { m_process.kill(); });

    connect(&m_process, &QProcess::readyReadStandardOutput, this, [this] {
        if (m_process.bytesAvailable() > kMaxOutput)
            m_process.kill();
    });
    connect(&m_process, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        m_killTimer.stop();
        const QByteArray out = m_process.read(kMaxOutput);
        const bool tooBig = m_process.bytesAvailable() > 0;
        m_process.readAll();
        emit busyChanged();
        handleFinished(tooBig ? QByteArray() : out, status == QProcess::NormalExit && code == 0 && !tooBig);
    });
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            m_killTimer.stop();
            emit busyChanged();
            handleFinished({}, false);
        }
    });

    m_pollTimer.setInterval(kPollMs);
    connect(&m_pollTimer, &QTimer::timeout, this, &HeadsetMic::refresh);
    m_pollTimer.start();
}

HeadsetMic::~HeadsetMic()
{
    // Same reason as HeadsetController: no signals into destroyed members.
    m_process.disconnect(this);
    if (m_process.state() != QProcess::NotRunning) {
        m_process.kill();
        m_process.waitForFinished(1000);
    }
}

void HeadsetMic::setUsbId(const QString &id)
{
    if (id == m_usbId)
        return;
    m_usbId = id;
    emit usbIdChanged();
    refresh();
}

void HeadsetMic::setPolling(bool enabled)
{
    if (enabled == m_pollTimer.isActive())
        return;
    if (enabled) {
        m_pollTimer.start();
        refresh();
    } else {
        m_pollTimer.stop();
    }
    emit pollingChanged();
}

void HeadsetMic::refresh()
{
    if (busy())
        return;
    if (m_usbId.size() != 9) {
        if (m_available) {
            m_available = false;
            emit changed();
        }
        return;
    }
    run({QStringLiteral("-f"), QStringLiteral("json"), QStringLiteral("list"), QStringLiteral("cards")}, false);
}

void HeadsetMic::setEnabled(bool enabled)
{
    if (!m_available || busy() || enabled == m_enabled)
        return;
    const QString profile = enabled ? m_profileWithMic : m_profileWithoutMic;
    if (!safeName(m_card) || !safeName(profile))
        return emit notify(tr("Unexpected sound card data"), true);
    m_targetEnabled = enabled;
    run({QStringLiteral("set-card-profile"), m_card, profile}, true);
}

void HeadsetMic::run(const QStringList &args, bool isSet)
{
    m_pendingSet = isSet;
    m_process.setArguments(args);
    m_killTimer.start();
    m_process.start(QIODevice::ReadOnly);
    emit busyChanged();
}

void HeadsetMic::handleFinished(const QByteArray &output, bool ok)
{
    if (m_pendingSet) {
        m_pendingSet = false;
        if (ok)
            emit notify(m_targetEnabled ? tr("Headset microphone enabled") : tr("Headset microphone disabled"), false);
        else
            emit notify(tr("Could not change the sound card profile"), true);
        refresh();
        return;
    }
    // A failed, hung or oversized read leaves nothing to switch: hide the option.
    parseCards(ok ? output : QByteArray());
}

void HeadsetMic::parseCards(const QByteArray &json)
{
    const bool wasAvailable = m_available;
    const bool wasEnabled = m_enabled;
    m_available = false;
    m_card.clear();
    m_profileWithMic.clear();
    m_profileWithoutMic.clear();

    const QString vendor = QStringLiteral("0x") + m_usbId.left(4);
    const QString product = QStringLiteral("0x") + m_usbId.mid(5);
    const QJsonArray cards = QJsonDocument::fromJson(json).array();
    for (const QJsonValue &value : cards) {
        const QJsonObject card = value.toObject();
        const QJsonObject props = card.value(QStringLiteral("properties")).toObject();
        if (props.value(QStringLiteral("device.vendor.id")).toString().toLower() != vendor
            || props.value(QStringLiteral("device.product.id")).toString().toLower() != product)
            continue;
        const QString name = card.value(QStringLiteral("name")).toString();
        if (!safeName(name))
            continue;

        const QString active = card.value(QStringLiteral("active_profile")).toString();
        const QString output = outputPart(active);
        const QJsonObject profiles = card.value(QStringLiteral("profiles")).toObject();

        // Best profile with and without input, preferring the current output mode.
        QString withMic, withoutMic;
        int withMicScore = -1, withoutMicScore = -1;
        for (auto it = profiles.begin(); it != profiles.end(); ++it) {
            const QJsonObject p = it.value().toObject();
            if (!p.value(QStringLiteral("available")).toBool(true) || !safeName(it.key()))
                continue;
            const int sinks = p.value(QStringLiteral("sinks")).toInt();
            const int sources = p.value(QStringLiteral("sources")).toInt();
            const int priority = qBound(0, p.value(QStringLiteral("priority")).toInt(), 1 << 20);
            if (sinks <= 0)
                continue;
            if (sources > 0) {
                const bool sameOutput = !output.isEmpty() && it.key().startsWith(output + QStringLiteral("+input:"));
                const int score = priority + (sameOutput ? (1 << 21) : 0);
                if (score > withMicScore) {
                    withMicScore = score;
                    withMic = it.key();
                }
            } else {
                const bool sameOutput = !output.isEmpty() && it.key() == output;
                const int score = priority + (sameOutput ? (1 << 21) : 0);
                if (score > withoutMicScore) {
                    withoutMicScore = score;
                    withoutMic = it.key();
                }
            }
        }
        if (withMic.isEmpty() || withoutMic.isEmpty())
            continue;

        m_card = name;
        m_profileWithMic = withMic;
        m_profileWithoutMic = withoutMic;
        m_enabled = profiles.value(active).toObject().value(QStringLiteral("sources")).toInt() > 0;
        m_available = true;
        break;
    }
    if (m_available != wasAvailable || m_enabled != wasEnabled)
        emit changed();
    emit cardsLoaded();
}
