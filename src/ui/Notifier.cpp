/*
 * Notifier.cpp - audible and visual notification of received traffic.
 *
 * This file is part of AX25Chat.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "Notifier.h"

#include "AndroidNotify.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QUrl>

#ifdef AX25CHAT_HAVE_MULTIMEDIA
#include <QSoundEffect>
#endif

#ifdef Q_OS_WIN
#include <windows.h>
#include <mmsystem.h>
#endif

namespace {

qint64 monotonicMs()
{
    static QElapsedTimer timer;
    if (!timer.isValid()) timer.start();
    return timer.elapsed();
}

} // namespace

QList<std::pair<QString, QString>> Notifier::categories()
{
    return {
        {QStringLiteral("message"), QStringLiteral("APRS message addressed to me")},
        {QStringLiteral("chat"), QStringLiteral("Chat text on the unproto destination")},
        {QStringLiteral("bulletin"), QStringLiteral("Bulletin or announcement")},
        {QStringLiteral("weather"), QStringLiteral("Weather bulletin")},
        {QStringLiteral("position"), QStringLiteral("Position report from another station")},
        {QStringLiteral("status"), QStringLiteral("Status report")},
    };
}

Notifier::Notifier(const AppConfig &config, QWidget *window, QObject *parent)
    : QObject(parent), m_config(config), m_window(window)
{
}

void Notifier::reloadConfig(const AppConfig &config)
{
    m_config = config;
    // The sound file may have changed; drop the cached player.
    if (m_effect) {
        m_effect->deleteLater();
        m_effect = nullptr;
    }
    m_effectPath.clear();
}

bool Notifier::enabledFor(const QString &category) const
{
    const NotificationConfig &n = m_config.notifications;
    if (category == QLatin1String("message")) return n.onMessage;
    if (category == QLatin1String("chat")) return n.onChat;
    if (category == QLatin1String("bulletin")) return n.onBulletin;
    if (category == QLatin1String("weather")) return n.onWeather;
    if (category == QLatin1String("position")) return n.onPosition;
    if (category == QLatin1String("status")) return n.onStatus;
    return false;
}

bool Notifier::rateLimited(const QString &category)
{
    const int interval = qMax(0, m_config.notifications.minIntervalS);
    if (interval <= 0) return false;
    const qint64 now = monotonicMs();
    const qint64 previous = m_last.value(category, -1);
    if (previous >= 0 && now - previous < interval * 1000) return true;
    m_last.insert(category, now);
    return false;
}

bool Notifier::notify(const QString &category, const QString &summary, const QString &message)
{
    const NotificationConfig &settings = m_config.notifications;
    if (!settings.enabled || !enabledFor(category)) return false;
    if (rateLimited(category)) return false;
    if (settings.sound) play();
    if (settings.flashWindow && m_window) {
        // Only when the window is not in front; flashing a window the
        // operator is already looking at is pure noise.
        if (!m_window->isActiveWindow()) QApplication::alert(m_window);
    }
    // A phone with the application off screen: the system's notification.
    if (androidnotify::available() && QApplication::applicationState() != Qt::ApplicationActive) {
        androidnotify::show(summary.isEmpty() ? QStringLiteral("AX25Chat") : summary, message);
    }
    return true;
}

void Notifier::test()
{
    play();
}

// ---- the sound ------------------------------------------------------------------------

QString Notifier::soundSource() const
{
    const QString path = m_config.notifications.soundFile.trimmed();
    if (path.isEmpty()) return QStringLiteral("qrc:/sounds/notify.wav");
    return QFileInfo(path).isFile() ? QUrl::fromLocalFile(path).toString() : QString();
}

void Notifier::play()
{
    // Android: the phone's own notification sound, through Java.
    if (androidnotify::available()) {
        androidnotify::beep();
        return;
    }
    const QString source = soundSource();
    if (source.isEmpty()) {
        emit logMessage(QStringLiteral("error"),
                        QStringLiteral("Notification sound not found: %1 (leave the field empty for the built-in chime)")
                            .arg(m_config.notifications.soundFile.trimmed()));
        QApplication::beep();
        return;
    }
#ifdef AX25CHAT_HAVE_MULTIMEDIA
    // AX25CHAT_SOUND_PLAYER=platform skips QtMultimedia, for testing the
    // fallback.
    if (qgetenv("AX25CHAT_SOUND_PLAYER") != "platform") {
        if (QSoundEffect *effect = loadEffect(source)) {
            if (effect->status() == QSoundEffect::Error) {
                // A format or a backend the effect cannot handle: the
                // platform's player instead.
                if (!playWithPlatform(source)) QApplication::beep();
                return;
            }
            effect->play();
            return;
        }
    }
#endif
    if (!playWithPlatform(source)) QApplication::beep();
}

QSoundEffect *Notifier::loadEffect(const QString &source)
{
#ifdef AX25CHAT_HAVE_MULTIMEDIA
    if (m_effect && m_effectPath == source) return m_effect;
    if (m_effect) m_effect->deleteLater();
    m_effect = new QSoundEffect(this);
    m_effect->setSource(QUrl(source));
    m_effect->setVolume(qBound(0.0, m_config.notifications.volume / 100.0, 1.0));
    m_effectPath = source;
    return m_effect;
#else
    Q_UNUSED(source);
    return nullptr;
#endif
}

// Without QtMultimedia: the platform's own means.  The built-in chime is
// written out once for the players that want a file.
bool Notifier::playWithPlatform(const QString &source)
{
    QString path;
    if (source.startsWith(QLatin1String("qrc:"))) {
        if (m_tempCopy.isEmpty() || !QFileInfo::exists(m_tempCopy)) {
            const QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
            m_tempCopy = dir + QStringLiteral("/ax25chat-notify.wav");
            QFile::remove(m_tempCopy);
            QFile::copy(QStringLiteral(":/sounds/notify.wav"), m_tempCopy);
            QFile::setPermissions(m_tempCopy, QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup | QFile::ReadOther);
        }
        path = m_tempCopy;
    } else {
        path = QUrl(source).toLocalFile();
    }
    if (path.isEmpty() || !QFileInfo::exists(path)) return false;

#ifdef Q_OS_WIN
    // winmm's PlaySound: asynchronous, no dependency beyond what the modem
    // already links for its audio.
    const std::wstring wide = path.toStdWString();
    return PlaySoundW(wide.c_str(), nullptr, SND_FILENAME | SND_ASYNC | SND_NODEFAULT) != FALSE;
#else
    // The first player at hand: PipeWire, PulseAudio, ALSA, or macOS's.
    struct Player { const char *program; QStringList args; };
    const int volume = qBound(0, m_config.notifications.volume, 100);
    const QList<Player> players = {
        {"pw-play", {QStringLiteral("--volume=%1").arg(volume / 100.0, 0, 'f', 2), path}},
        {"paplay", {QStringLiteral("--volume=%1").arg(volume * 65536 / 100), path}},
        {"aplay", {QStringLiteral("-q"), path}},
        {"afplay", {QStringLiteral("-v"), QStringLiteral("%1").arg(volume / 100.0, 0, 'f', 2), path}},
    };
    for (const Player &player : players) {
        const QString program = QStandardPaths::findExecutable(QString::fromLatin1(player.program));
        if (program.isEmpty()) continue;
        if (QProcess::startDetached(program, player.args)) return true;
    }
    if (!m_warnedNoPlayer) {
        m_warnedNoPlayer = true;
        emit logMessage(QStringLiteral("info"),
                        QStringLiteral("No sound player found (pw-play, paplay, aplay or afplay) and this build has no "
                                       "QtMultimedia: the system beep is used, which most desktops keep silent."));
    }
    return false;
#endif
}
