/*
 * Notifier.h - audible and visual notification of received traffic.
 *
 * Port of ax25chat/notifications.py.  Sound is selectable per category
 * because the categories differ enormously in how often they arrive: a
 * directed message is rare and worth interrupting for; positions on a busy
 * APRS frequency arrive every few seconds.  Every category is rate limited
 * regardless, per category, so a flood of positions cannot mask a message.
 *
 * The sound is the WAV file the settings name, or the chime compiled into
 * the program (assets/sounds/notify.wav) when none is. Both go through
 * QSoundEffect when the build has QtMultimedia, and otherwise through what
 * the platform offers - PlaySound on Windows, pw-play/paplay/aplay on
 * Linux, afplay on macOS - before the last resort, the system beep, which
 * most desktops keep silent. On Android the sound and the notification go
 * through the Java side (the phone's own notification sound).
 *
 * This file is part of AX25Chat.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "AppConfig.h"

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QWidget>

class QSoundEffect;

class Notifier : public QObject
{
    Q_OBJECT

public:
    // Categories a notification can belong to, with their descriptions.
    static QList<std::pair<QString, QString>> categories();

    explicit Notifier(const AppConfig &config, QWidget *window = nullptr, QObject *parent = nullptr);

    void reloadConfig(const AppConfig &config);
    bool enabledFor(const QString &category) const;

    // Alert for one received item.  Returns whether anything was played.
    // On Android, with the application off screen, the message goes into
    // a system notification as well.
    bool notify(const QString &category, const QString &summary = QString(), const QString &message = QString());

    // Play the configured sound once, ignoring the rate limit.
    void test();

signals:
    void logMessage(const QString &level, const QString &message);

private:
    bool rateLimited(const QString &category);
    void play();
    // The sound to play: the configured file as a file URL, the built-in
    // chime as a qrc URL, or empty when the configured file is missing.
    QString soundSource() const;
    bool playWithPlatform(const QString &source);
    QSoundEffect *loadEffect(const QString &source);

    AppConfig m_config;
    QPointer<QWidget> m_window;
    QHash<QString, qint64> m_last;
    QSoundEffect *m_effect = nullptr;
    QString m_effectPath;
    QString m_tempCopy;        // the built-in chime on disk, for external players
    bool m_warnedNoPlayer = false;
};
