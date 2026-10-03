/*
 * MainWindow.h - the desktop window, a Qt Widgets view over a Session.
 *
 * The window renders the session's log entries, lists and state and calls
 * its actions; it holds no protocol logic of its own. It uses the
 * platform's native Qt style: no stylesheet, standard QMainWindow parts, so
 * it follows the desktop theme including a light/dark switch made while
 * running. The only colours chosen explicitly are the ones that carry
 * meaning (Style.h), re-resolved whenever the palette changes.
 *
 * This file is part of AX25Chat.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "AppConfig.h"
#include "ChannelManager.h"
#include "Session.h"

#include <QHash>
#include <QLabel>
#include <QMainWindow>
#include <QString>

class MapWidget;
class Notifier;
class SettingsPage;
class QAction;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QTabWidget;
class QTextBrowser;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(const AppConfig &config, QWidget *parent = nullptr);
    ~MainWindow() override;

    static QString version() { return Session::version(); }
    Session *session() const { return m_session; }

    // Explain what a fresh installation configured for itself.
    void noteFirstRun(const QString &confPath) { m_session->noteFirstRun(confPath); }

protected:
    void changeEvent(QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    void buildUi();
    void buildToolbar();
    QWidget *buildChatPage();
    void buildStatusBar();
    QLabel *panelLabel(const QString &text);
    QLabel *registerHint(QLabel *label);
    void connectSession();

    void onSend();
    void clearChat();
    void saveChat();
    void onLogEntry(const QString &prefix, const QString &message, const QString &colourKey);
    void onModemState();
    void onChannelState();
    void onQueueChanged(int count);
    void applySettings(const AppConfig &config);
    void refreshStations();
    void refreshPanel();
    void insertCallsign(QListWidgetItem *item);
    QString timestamp() const;
    void append(const QString &prefix, const QString &message, const QString &colourKey);

    Session *m_session = nullptr;
    Notifier *m_notifier = nullptr;
    AppConfig m_config;
    QHash<QString, QString> m_colours;
    QList<QLabel *> m_hintLabels;

    QTabWidget *m_tabs = nullptr;
    SettingsPage *m_settingsPage = nullptr;
    MapWidget *m_map = nullptr;
    QTextBrowser *m_logView = nullptr;
    QLineEdit *m_recipientEdit = nullptr;
    QLineEdit *m_inputEdit = nullptr;
    QPushButton *m_sendButton = nullptr;
    QListWidget *m_heardList = nullptr;
    QLabel *m_heardTitle = nullptr;
    QListWidget *m_queueList = nullptr;
    QLabel *m_beaconLabel = nullptr;
    QLabel *m_positionLabel = nullptr;
    QLabel *m_gpsLabel = nullptr;
    QLabel *m_channelIndicator = nullptr;
    QLabel *m_modemLabel = nullptr;
    QLabel *m_queueLabel = nullptr;
    QLabel *m_aprsIsLabel = nullptr;
    QLabel *m_stationLabel = nullptr;
    QAction *m_modemAction = nullptr;
    QAction *m_cancelAction = nullptr;
};
