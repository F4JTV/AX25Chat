/*
 * MainWindow.cpp - the desktop window, a Qt Widgets view over a Session.
 *
 * This file is part of AX25Chat.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "MainWindow.h"

#include <algorithm>

#include "MapWidget.h"
#include "Notifier.h"
#include "SettingsPage.h"
#include "Style.h"
#include "SymbolArt.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollBar>
#include <QSplitter>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTabWidget>
#include <QTextBrowser>
#include <QTextDocument>
#include <QToolBar>
#include <QVBoxLayout>

namespace {

// The conversation colour for a log entry's key, and the notification
// category it belongs to.
QString colourFor(const QString &key)
{
    if (key == QLatin1String("message")) return QStringLiteral("rx");
    if (key == QLatin1String("bulletin")) return QStringLiteral("beacon");
    if (key == QLatin1String("weather") || key == QLatin1String("position") || key == QLatin1String("status")) {
        return QStringLiteral("monitor");
    }
    return key;
}

QString categoryFor(const QString &key)
{
    if (key == QLatin1String("rx")) return QStringLiteral("chat");
    if (key == QLatin1String("message") || key == QLatin1String("bulletin") || key == QLatin1String("weather")
        || key == QLatin1String("position") || key == QLatin1String("status")) {
        return key;
    }
    return QString();
}

} // namespace


MainWindow::MainWindow(const AppConfig &config, QWidget *parent)
    : QMainWindow(parent), m_config(config)
{
    m_colours = style::colours(palette());
    setWindowTitle(QStringLiteral("AX25Chat %1 - packet radio chat over AX.25").arg(version()));
    resize(1080, 700);

    m_notifier = new Notifier(config, this, this);
    buildUi();

    // The session starts working as soon as it exists (auto-start of the
    // modem after 300 ms), so the view is wired before it is created and
    // its first log lines are queued to us.
    m_session = new Session(config, this);
    connectSession();
    onModemState();
    onChannelState();
    refreshPanel();
}

MainWindow::~MainWindow() = default;


// ---- user interface ---------------------------------------------------------------

void MainWindow::buildUi()
{
    m_tabs = new QTabWidget(this);
    m_tabs->addTab(buildChatPage(), QStringLiteral("&Chat"));
    m_map = new MapWidget(m_config, this);
    m_tabs->addTab(m_map, QStringLiteral("&Map"));
    connect(m_map, &MapWidget::stationClicked, this, [this](const QString &call) {
        m_recipientEdit->setText(call);
        m_tabs->setCurrentIndex(0);
        m_inputEdit->setFocus();
    });
    m_settingsPage = new SettingsPage(m_config, this);
    m_tabs->addTab(m_settingsPage, QStringLiteral("Confi&guration"));
    setCentralWidget(m_tabs);
    buildToolbar();
    buildStatusBar();
    connect(m_settingsPage, &SettingsPage::settingsSaved, this, &MainWindow::applySettings);
}

void MainWindow::buildToolbar()
{
    auto *toolbar = new QToolBar(QStringLiteral("Main"), this);
    toolbar->setMovable(false);
    toolbar->setToolButtonStyle(Qt::ToolButtonTextOnly);
    addToolBar(toolbar);

    m_modemAction = new QAction(QStringLiteral("Start"), this);
    m_modemAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+K")));
    m_modemAction->setStatusTip(QStringLiteral("Start or stop the station: the built-in modem and the APRS-IS connection"));
    connect(m_modemAction, &QAction::triggered, this, [this] { m_session->toggleStation(); });
    toolbar->addAction(m_modemAction);
    toolbar->addSeparator();

    auto *beacon = new QAction(QStringLiteral("Beacon now"), this);
    beacon->setStatusTip(QStringLiteral("Queue a beacon; it goes out when the frequency is clear"));
    connect(beacon, &QAction::triggered, this, [this] { m_session->beaconNow(); });
    toolbar->addAction(beacon);

    auto *position = new QAction(QStringLiteral("Send position"), this);
    position->setStatusTip(QStringLiteral("Queue an APRS position report; it goes out when the frequency is clear"));
    connect(position, &QAction::triggered, this, [this] { m_session->positionNow(); });
    toolbar->addAction(position);

    m_cancelAction = new QAction(QStringLiteral("Cancel queue"), this);
    m_cancelAction->setStatusTip(QStringLiteral("Discard everything waiting to transmit"));
    m_cancelAction->setEnabled(false);
    connect(m_cancelAction, &QAction::triggered, this, [this] { m_session->cancelQueue(); });
    toolbar->addAction(m_cancelAction);
    toolbar->addSeparator();

    auto *save = new QAction(QStringLiteral("Save chat"), this);
    save->setShortcut(QKeySequence::Save);
    save->setStatusTip(QStringLiteral("Write the conversation to a text file"));
    connect(save, &QAction::triggered, this, &MainWindow::saveChat);
    toolbar->addAction(save);

    auto *clear = new QAction(QStringLiteral("Clear chat"), this);
    clear->setShortcut(QKeySequence(QStringLiteral("Ctrl+L")));
    clear->setStatusTip(QStringLiteral("Erase the conversation window"));
    connect(clear, &QAction::triggered, this, &MainWindow::clearChat);
    toolbar->addAction(clear);
}

QWidget *MainWindow::buildChatPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    auto *splitter = new QSplitter(Qt::Horizontal, page);

    auto *left = new QWidget(splitter);
    auto *leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(0, 0, 0, 0);

    m_logView = new QTextBrowser(left);
    m_logView->setOpenExternalLinks(false);
    m_logView->document()->setMaximumBlockCount(m_config.ui.maxLogLines);
    // The platform's own fixed-width font, not an imposed family.
    QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    if (m_config.ui.fontSize > 0) mono.setPointSize(m_config.ui.fontSize);
    m_logView->setFont(mono);
    leftLayout->addWidget(m_logView, 1);

    auto *inputRow = new QHBoxLayout;
    m_recipientEdit = new QLineEdit(left);
    m_recipientEdit->setPlaceholderText(QStringLiteral("To (blank = all)"));
    m_recipientEdit->setFixedWidth(150);
    m_recipientEdit->setToolTip(QStringLiteral(
        "Leave blank to send to everyone on the unproto destination.\n"
        "Enter a callsign to send an APRS message to that station, which "
        "will be acknowledged and retried until it gets through."));
    connect(m_recipientEdit, &QLineEdit::returnPressed, this, &MainWindow::onSend);
    inputRow->addWidget(m_recipientEdit);

    m_inputEdit = new QLineEdit(left);
    m_inputEdit->setPlaceholderText(QStringLiteral("Type a message, Enter to send. It is held back while the frequency is busy."));
    connect(m_inputEdit, &QLineEdit::returnPressed, this, &MainWindow::onSend);
    inputRow->addWidget(m_inputEdit, 1);

    m_sendButton = new QPushButton(QStringLiteral("&Send"), left);
    m_sendButton->setDefault(true);
    m_sendButton->setAutoDefault(true);
    connect(m_sendButton, &QPushButton::clicked, this, &MainWindow::onSend);
    inputRow->addWidget(m_sendButton);
    leftLayout->addLayout(inputRow);
    splitter->addWidget(left);

    auto *side = new QWidget(splitter);
    auto *sideLayout = new QVBoxLayout(side);
    sideLayout->setContentsMargins(0, 0, 0, 0);
    m_heardTitle = panelLabel(QStringLiteral("Stations heard"));
    sideLayout->addWidget(m_heardTitle);
    m_heardList = new QListWidget(side);
    m_heardList->setToolTip(QStringLiteral("Double-click a station to address a message to it"));
    if (symbolart::available()) m_heardList->setIconSize(QSize(24, 24));
    connect(m_heardList, &QListWidget::itemDoubleClicked, this, &MainWindow::insertCallsign);
    sideLayout->addWidget(m_heardList, 3);
    sideLayout->addWidget(panelLabel(QStringLiteral("Transmit queue")));
    m_queueList = new QListWidget(side);
    sideLayout->addWidget(m_queueList, 2);
    m_beaconLabel = registerHint(new QLabel(QStringLiteral("Beacon: off"), side));
    sideLayout->addWidget(m_beaconLabel);
    m_positionLabel = registerHint(new QLabel(QStringLiteral("Position: off"), side));
    sideLayout->addWidget(m_positionLabel);
    m_gpsLabel = registerHint(new QLabel(QString(), side));
    m_gpsLabel->setWordWrap(true);
    sideLayout->addWidget(m_gpsLabel);
    splitter->addWidget(side);

    splitter->setStretchFactor(0, 4);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({760, 300});
    layout->addWidget(splitter);
    return page;
}

QLabel *MainWindow::panelLabel(const QString &text)
{
    auto *label = new QLabel(text, this);
    QFont font = label->font();
    font.setBold(true);
    label->setFont(font);
    return label;
}

QLabel *MainWindow::registerHint(QLabel *label)
{
    style::applyHintStyle(label);
    m_hintLabels.append(label);
    return label;
}

void MainWindow::buildStatusBar()
{
    QStatusBar *bar = statusBar();
    m_channelIndicator = new QLabel(bar);
    bar->addWidget(m_channelIndicator);
    m_modemLabel = new QLabel(QStringLiteral("Modem: stopped"), bar);
    bar->addPermanentWidget(m_modemLabel);
    m_aprsIsLabel = new QLabel(bar);
    bar->addPermanentWidget(m_aprsIsLabel);
    m_queueLabel = new QLabel(QStringLiteral("Queue: 0"), bar);
    bar->addPermanentWidget(m_queueLabel);
    m_stationLabel = new QLabel(m_config.myCall(), bar);
    QFont font = m_stationLabel->font();
    font.setBold(true);
    m_stationLabel->setFont(font);
    bar->addPermanentWidget(m_stationLabel);
}

void MainWindow::connectSession()
{
    connect(m_session, &Session::logEntry, this, &MainWindow::onLogEntry);
    connect(m_session, &Session::modemStateChanged, this, &MainWindow::onModemState);
    connect(m_session, &Session::stationChanged, this, &MainWindow::onModemState);
    connect(m_session, &Session::channelStateChanged, this, &MainWindow::onChannelState);
    connect(m_session, &Session::queueChanged, this, &MainWindow::onQueueChanged);
    connect(m_session, &Session::stationsChanged, this, &MainWindow::refreshStations);
    connect(m_session, &Session::panelChanged, this, &MainWindow::refreshPanel);
    connect(m_session, &Session::configurationNeeded, this, [this] { m_tabs->setCurrentIndex(2); });
    connect(m_settingsPage, &SettingsPage::testSoundRequested, m_notifier, &Notifier::test);
}

// Follow the desktop theme if it changes while we are running.
void MainWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::PaletteChange) {
        m_colours = style::colours(palette());
        onChannelState();
        for (QLabel *label : m_hintLabels) style::applyHintStyle(label);
    }
}


// ---- session feedback ---------------------------------------------------------------

void MainWindow::onLogEntry(const QString &prefix, const QString &message, const QString &colourKey)
{
    const QString category = categoryFor(colourKey);
    if (!category.isEmpty()) m_notifier->notify(category, prefix);
    append(prefix, message, colourFor(colourKey));
}

void MainWindow::onModemState()
{
    m_modemLabel->setText(m_session->modemStatusText());
    m_modemAction->setText(m_session->stationOn() ? QStringLiteral("Stop") : QStringLiteral("Start"));
    m_modemAction->setEnabled(!m_session->stationBusy());
}

// The channel state as a coloured bullet plus plain text.  Only the bullet
// is coloured, so the label keeps the theme's own text colour.
void MainWindow::onChannelState()
{
    const QString colour = m_colours.value(m_session->channelColourKey());
    m_channelIndicator->setText(QStringLiteral("<span style='color:%1'>&#9679;</span> %2")
                                    .arg(colour, m_session->channelStateLabel().toHtmlEscaped()));
}

void MainWindow::onQueueChanged(int count)
{
    m_queueLabel->setText(QStringLiteral("Queue: %1").arg(count));
    m_cancelAction->setEnabled(count > 0);
    m_queueList->clear();
    ChannelManager *channel = m_session->channel();
    const auto items = channel->pendingItems();
    for (const ChannelManager::OutgoingItem &item : items) {
        const qint64 waited = qMax<qint64>(0, (channel->clockNow() - item.queuedAtMs) / 1000);
        auto *entry = new QListWidgetItem(QStringLiteral("[%1] %2  (%3s)").arg(item.kind, item.describe()).arg(waited));
        entry->setToolTip(QStringLiteral("Waiting for a clear frequency"));
        m_queueList->addItem(entry);
    }
}

// Every second for the ages, and whenever a station is heard: the list is
// alphabetical, and changes are applied in place - rows updated, a
// newcomer inserted where it sorts, an expired station removed - so the
// scroll position and the selection survive.
void MainWindow::refreshStations()
{
    const auto reference = m_session->referencePosition();
    const StationRegistry &registry = m_session->stations();
    const double now = registry.now();
    QList<Station> stations = registry.listed();
    m_map->setStations(stations);
    m_map->setOwnPosition(reference, m_config.myCall(), m_config.position.symbolTable, m_config.position.symbolCode);
    std::sort(stations.begin(), stations.end(), [](const Station &a, const Station &b) {
        return a.callsign.compare(b.callsign, Qt::CaseInsensitive) < 0;
    });
    m_heardTitle->setText(stations.isEmpty() ? QStringLiteral("Stations heard")
                                             : QStringLiteral("Stations heard (%1)").arg(stations.size()));
    auto fill = [&](QListWidgetItem *entry, const Station &station) {
        entry->setText(station.describe(now, reference));
        entry->setToolTip(station.tooltip());
        entry->setData(Qt::UserRole, station.callsign);
        if (station.hasPosition() && entry->icon().isNull()) {
            const QIcon art = symbolart::icon(station.symbolTable, station.symbolCode, 24);
            if (!art.isNull()) entry->setIcon(art);
        }
    };
    int i = 0, j = 0;
    while (i < m_heardList->count() || j < stations.size()) {
        const QString current = i < m_heardList->count() ? m_heardList->item(i)->data(Qt::UserRole).toString() : QString();
        const int order = (i >= m_heardList->count()) ? 1 : (j >= stations.size()) ? -1
                        : current.compare(stations.at(j).callsign, Qt::CaseInsensitive);
        if (order < 0) {
            delete m_heardList->takeItem(i);
        } else if (order > 0) {
            auto *entry = new QListWidgetItem(QString());
            fill(entry, stations.at(j));
            m_heardList->insertItem(i, entry);
            i++;
            j++;
        } else {
            fill(m_heardList->item(i), stations.at(j));
            i++;
            j++;
        }
    }
}

void MainWindow::refreshPanel()
{
    const QString isText = m_session->aprsIsStatusText();
    m_aprsIsLabel->setText(isText);
    m_aprsIsLabel->setVisible(!isText.isEmpty());
    m_beaconLabel->setText(m_session->beaconCountdown());
    m_positionLabel->setText(m_session->positionCountdown());
    m_gpsLabel->setText(m_session->gpsStatusText());
    if (m_session->queueCount()) onQueueChanged(m_session->queueCount());
}

// Double-clicking a station addresses the next message to it.
void MainWindow::insertCallsign(QListWidgetItem *item)
{
    QString call = item->data(Qt::UserRole).toString();
    if (call.isEmpty()) call = item->text().section(' ', 0, 0);
    m_recipientEdit->setText(call);
    m_inputEdit->setFocus();
}


// ---- actions ----------------------------------------------------------------------------

void MainWindow::onSend()
{
    if (m_session->sendText(m_recipientEdit->text(), m_inputEdit->text())) m_inputEdit->clear();
}

// Confirmed when there is something to lose: the log lives in memory and
// is not written anywhere unless saved.
void MainWindow::clearChat()
{
    if (!m_logView->document()->isEmpty()) {
        const auto answer = QMessageBox::question(
            this, QStringLiteral("Clear the chat"),
            QStringLiteral("Erase the conversation window?\n\nThis cannot be undone, and nothing is written to "
                           "disk unless you save it first. Stations heard and the transmit queue are not affected."),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes) return;
    }
    m_logView->clear();
    m_session->systemLine(QStringLiteral("Chat cleared. Station %1.").arg(m_config.myCall()));
}

void MainWindow::saveChat()
{
    const QString text = m_logView->toPlainText();
    if (text.trimmed().isEmpty()) {
        m_session->warn(QStringLiteral("There is nothing in the chat window to save."));
        return;
    }
    const QString defaultName = QStringLiteral("ax25chat-%1-%2.txt")
        .arg(QString(m_config.myCall()).replace('/', '_'), QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss")));
    QString folder = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (folder.isEmpty()) folder = QDir::homePath();
    QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Save the chat"), QDir(folder).filePath(defaultName),
                                                QStringLiteral("Text files (*.txt);;All files (*)"));
    if (path.isEmpty()) return;
    if (QFileInfo(path).suffix().isEmpty()) path += QStringLiteral(".txt");

    const QString header = QStringLiteral(
        "AX25Chat %1 conversation log\n"
        "Station    : %2\n"
        "Unproto    : %3%4\n"
        "Saved      : %5\n"
        "Lines kept : %6 (the window holds at most %7; anything older has already scrolled out)\n"
        "%8\n\n")
        .arg(version(), m_config.myCall(), m_config.station.destination,
             m_config.station.digipeaters.isEmpty() ? QString() : QStringLiteral(" via %1").arg(m_config.station.digipeaters),
             QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss t")))
        .arg(m_logView->document()->blockCount()).arg(m_config.ui.maxLogLines)
        .arg(QString(72, '-'));

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, QStringLiteral("Could not save"), file.errorString());
        return;
    }
    file.write(header.toUtf8());
    file.write(text.toUtf8());
    file.write("\n");
    file.close();
    m_session->systemLine(QStringLiteral("Chat saved to %1").arg(path));
}

void MainWindow::applySettings(const AppConfig &config)
{
    m_config = config;
    m_notifier->reloadConfig(config);
    m_map->reloadConfig(config);
    m_stationLabel->setText(config.myCall());
    m_logView->document()->setMaximumBlockCount(config.ui.maxLogLines);
    m_session->applySettings(config);
    m_tabs->setCurrentIndex(0);
}


// ---- log -------------------------------------------------------------------------------------

QString MainWindow::timestamp() const
{
    if (!m_config.ui.showTimestamps) return QString();
    return QStringLiteral("<span style='color:%1'>%2</span> ")
        .arg(m_colours.value(QStringLiteral("dim")), QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")));
}

// Only the prefix is coloured; the message keeps the theme's own text
// colour so it stays readable under any palette.
void MainWindow::append(const QString &prefix, const QString &message, const QString &colourKey)
{
    const QString colour = m_colours.value(colourKey, m_colours.value(QStringLiteral("system")));
    QString safe = message.toHtmlEscaped();
    safe.replace(QStringLiteral("\n"), QStringLiteral("<br>&nbsp;&nbsp;"));
    m_logView->append(QStringLiteral("%1<span style='color:%2'><b>%3</b></span> %4")
                          .arg(timestamp(), colour, prefix.toHtmlEscaped(), safe));
    QScrollBar *bar = m_logView->verticalScrollBar();
    bar->setValue(bar->maximum());
}


// ---- closing ---------------------------------------------------------------------------------

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_session->queueCount()) {
        const auto answer = QMessageBox::question(
            this, QStringLiteral("Frames still queued"),
            QStringLiteral("%1 frame(s) are still waiting for a clear frequency. Quit anyway?").arg(m_session->queueCount()));
        if (answer != QMessageBox::Yes) {
            event->ignore();
            return;
        }
    }
    m_session->stopModem();
    event->accept();
}
