/*
 * SettingsPage.cpp - the Configuration tab.
 *
 * This file is part of AX25Chat.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "SettingsPage.h"

#include "AX25Frame.h"
#include "AprsEncoder.h"
#include "TileCache.h"
#include "TileProviders.h"
#include "GpsReceiver.h"
#include "ModemConfigFile.h"
#include "SmartBeacon.h"
#include "Style.h"
#include "SymbolArt.h"
#include "SymbolPicker.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QRegularExpressionValidator>
#include <QSpinBox>
#include <QStyleFactory>
#include <QUrl>
#include <QVBoxLayout>

namespace {

QLabel *hint(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setWordWrap(true);
    style::applyHintStyle(label);
    return label;
}

QSpinBox *spin(int min, int max, const QString &suffix, QWidget *parent)
{
    auto *s = new QSpinBox(parent);
    s->setRange(min, max);
    if (!suffix.isEmpty()) s->setSuffix(suffix);
    return s;
}

QDoubleSpinBox *dspin(double min, double max, int decimals, const QString &suffix, QWidget *parent)
{
    auto *s = new QDoubleSpinBox(parent);
    s->setRange(min, max);
    s->setDecimals(decimals);
    if (!suffix.isEmpty()) s->setSuffix(suffix);
    return s;
}

} // namespace


SettingsPage::SettingsPage(const AppConfig &config, QWidget *parent)
    : QWidget(parent), m_config(config)
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    // One column, the sections one under the other, as the Python version
    // laid them out: read top to bottom, scrolled as a whole.
    auto *content = new QWidget(scroll);
    auto *column = new QVBoxLayout(content);
    column->setSpacing(12);
    for (QWidget *group : {buildStationGroup(), buildModemGroup(), buildChannelGroup(), buildBeaconGroup(),
                           buildPositionGroup(), buildMessagingGroup(), buildAprsIsGroup(),
                           buildNotificationGroup(), buildInterfaceGroup()}) {
        column->addWidget(group);
    }
    column->addStretch(1);
    scroll->setWidget(content);
    outer->addWidget(scroll, 1);

    auto *bottom = new QHBoxLayout;
    m_pathLabel = hint(QStringLiteral("Settings file: %1").arg(AppConfig::configPath()), this);
    bottom->addWidget(m_pathLabel, 1);
    auto *save = new QPushButton(QStringLiteral("&Save configuration"), this);
    save->setDefault(false);
    save->setAutoDefault(false);
    connect(save, &QPushButton::clicked, this, &SettingsPage::onSave);
    bottom->addWidget(save);
    outer->addLayout(bottom);

    load(config);
}


// ---- groups ------------------------------------------------------------------------

QWidget *SettingsPage::buildStationGroup()
{
    auto *group = new QGroupBox(QStringLiteral("Station"), this);
    auto *form = new QFormLayout(group);
    m_callsignEdit = new QLineEdit(group);
    m_callsignEdit->setMaxLength(6);
    m_callsignEdit->setPlaceholderText(QStringLiteral("MYCALL"));
    form->addRow(QStringLiteral("Callsign"), m_callsignEdit);
    m_ssidSpin = spin(0, 15, QString(), group);
    form->addRow(QStringLiteral("SSID"), m_ssidSpin);
    m_destinationEdit = new QLineEdit(group);
    m_destinationEdit->setToolTip(QStringLiteral("Destination address of the UI frames. Both stations must use the same value."));
    form->addRow(QStringLiteral("Unproto destination"), m_destinationEdit);
    m_digiEdit = new QLineEdit(group);
    m_digiEdit->setPlaceholderText(QStringLiteral("WIDE1-1,WIDE2-1 or empty for direct"));
    form->addRow(QStringLiteral("Digipeater path"), m_digiEdit);
    m_nameEdit = new QLineEdit(group);
    form->addRow(QStringLiteral("Operator name"), m_nameEdit);
    m_locatorEdit = new QLineEdit(group);
    // Callsigns, paths and locators are upper case on the air whatever the
    // operator types.
    for (QLineEdit *edit : {m_callsignEdit, m_destinationEdit, m_digiEdit, m_locatorEdit}) {
        connect(edit, &QLineEdit::textEdited, edit, [edit](const QString &text) {
            const QString upper = text.toUpper();
            if (upper != text) {
                const int pos = edit->cursorPosition();
                edit->setText(upper);
                edit->setCursorPosition(pos);
            }
        });
    }
    m_locatorEdit->setPlaceholderText(QStringLiteral("JN23AB"));
    form->addRow(QStringLiteral("Locator"), m_locatorEdit);
    m_encodingCombo = new QComboBox(group);
    m_encodingCombo->addItem(QStringLiteral("UTF-8 (between copies of this application)"), QStringLiteral("utf-8"));
    m_encodingCombo->addItem(QStringLiteral("ASCII (old terminals)"), QStringLiteral("ascii"));
    form->addRow(QStringLiteral("Text encoding"), m_encodingCombo);
    m_maxInfoSpin = spin(20, 2048, QStringLiteral(" octets"), group);
    m_maxInfoSpin->setToolTip(QStringLiteral("Longer messages are split across several UI frames"));
    form->addRow(QStringLiteral("Max frame payload"), m_maxInfoSpin);
    return group;
}

QWidget *SettingsPage::buildModemGroup()
{
    auto *group = new QGroupBox(QStringLiteral("Modem (built-in Dire Wolf)"), this);
    auto *layout = new QVBoxLayout(group);
    auto *form = new QFormLayout;
    m_modemEnabledCheck = new QCheckBox(QStringLiteral("Use the modem (the radio); off = the APRS Internet System alone"), group);
    m_modemEnabledCheck->setToolTip(QStringLiteral("Start brings up what is ticked here and in the APRS Internet System section: the modem, the connection, or both."));
    form->addRow(QString(), m_modemEnabledCheck);

    auto *fileRow = new QHBoxLayout;
    m_modemConfigEdit = new QLineEdit(group);
    m_modemConfigEdit->setPlaceholderText(QStringLiteral("direwolf.conf, blank for the default"));
    connect(m_modemConfigEdit, &QLineEdit::editingFinished, this, [this] { onModemConfigChosen(m_modemConfigEdit->text()); });
    fileRow->addWidget(m_modemConfigEdit, 1);
    auto *browse = new QPushButton(QStringLiteral("Browse..."), group);
    connect(browse, &QPushButton::clicked, this, &SettingsPage::browseModemConfig);
    fileRow->addWidget(browse);
    auto *create = new QPushButton(QStringLiteral("Create"), group);
    create->setToolTip(QStringLiteral("Write a minimal working direwolf.conf in your own settings folder"));
    connect(create, &QPushButton::clicked, this, &SettingsPage::createModemConfig);
    fileRow->addWidget(create);
    auto *edit = new QPushButton(QStringLiteral("Edit"), group);
    edit->setToolTip(QStringLiteral("Open the file in your text editor"));
    connect(edit, &QPushButton::clicked, this, &SettingsPage::editModemConfig);
    fileRow->addWidget(edit);
    form->addRow(QStringLiteral("Configuration file"), fileRow);

    m_modemChannelSpin = spin(0, 15, QString(), group);
    m_modemChannelSpin->setToolTip(QStringLiteral("The CHANNEL declared in direwolf.conf to transmit and listen on. Leave at 0 "
                                                  "unless the file declares more than one CHANNEL."));
    form->addRow(QStringLiteral("Radio channel"), m_modemChannelSpin);
    m_modemAutoStartCheck = new QCheckBox(QStringLiteral("Start the station (modem and APRS-IS) when the application opens"), group);
    form->addRow(QString(), m_modemAutoStartCheck);
    m_modemEchoCheck = new QCheckBox(QStringLiteral("Echo the modem's messages into the chat log"), group);
    form->addRow(QString(), m_modemEchoCheck);
    layout->addLayout(form);

    auto *paramsForm = new QFormLayout;
    m_sendParamsCheck = new QCheckBox(QStringLiteral("Apply these channel access parameters at start"), group);
    m_sendParamsCheck->setToolTip(QStringLiteral("Off: keep what direwolf.conf says"));
    paramsForm->addRow(QString(), m_sendParamsCheck);
    m_txdelaySpin = spin(0, 255, QStringLiteral(" x10 ms"), group);
    paramsForm->addRow(QStringLiteral("TXDELAY"), m_txdelaySpin);
    m_persistSpin = spin(0, 255, QString(), group);
    paramsForm->addRow(QStringLiteral("Persistence"), m_persistSpin);
    m_slottimeSpin = spin(0, 255, QStringLiteral(" x10 ms"), group);
    paramsForm->addRow(QStringLiteral("Slot time"), m_slottimeSpin);
    m_txtailSpin = spin(0, 255, QStringLiteral(" x10 ms"), group);
    paramsForm->addRow(QStringLiteral("TX tail"), m_txtailSpin);
    m_fullDuplexCheck = new QCheckBox(QStringLiteral("Full duplex (off on a simplex frequency)"), group);
    paramsForm->addRow(QString(), m_fullDuplexCheck);
    layout->addLayout(paramsForm);
    layout->addWidget(hint(QStringLiteral("Persistence and slot time are what make the modem hold the PTT while another "
                                          "station is transmitting. Audio device, PTT line and modem speed are set in "
                                          "direwolf.conf."), group));
    return group;
}

QWidget *SettingsPage::buildChannelGroup()
{
    auto *group = new QGroupBox(QStringLiteral("Channel access (application side)"), this);
    auto *form = new QFormLayout(group);
    m_useDcdCheck = new QCheckBox(QStringLiteral("Treat the modem's carrier detect as busy"), group);
    form->addRow(QString(), m_useDcdCheck);
    m_holdOffSpin = spin(0, 60000, QStringLiteral(" ms"), group);
    m_holdOffSpin->setToolTip(QStringLiteral("Quiet time required after the last frame or carrier before transmitting"));
    form->addRow(QStringLiteral("Hold-off after activity"), m_holdOffSpin);
    m_gapSpin = spin(0, 10000, QStringLiteral(" ms"), group);
    form->addRow(QStringLiteral("Gap between own frames"), m_gapSpin);
    m_jitterSpin = spin(0, 10000, QStringLiteral(" ms"), group);
    form->addRow(QStringLiteral("Random back-off, up to"), m_jitterSpin);
    m_maxDeferSpin = spin(0, 3600, QStringLiteral(" s"), group);
    m_maxDeferSpin->setSpecialValueText(QStringLiteral("never"));
    form->addRow(QStringLiteral("Abandon a frame after"), m_maxDeferSpin);
    m_waitTxbufCheck = new QCheckBox(QStringLiteral("Wait for the modem's transmit queue to drain between frames"), group);
    form->addRow(QString(), m_waitTxbufCheck);
    m_ownEchoCheck = new QCheckBox(QStringLiteral("Count our own digipeated echo as activity"), group);
    form->addRow(QString(), m_ownEchoCheck);
    return group;
}

QWidget *SettingsPage::buildBeaconGroup()
{
    auto *group = new QGroupBox(QStringLiteral("Beacon"), this);
    auto *form = new QFormLayout(group);
    m_beaconCheck = new QCheckBox(QStringLiteral("Send a beacon periodically"), group);
    form->addRow(QString(), m_beaconCheck);
    m_beaconIntervalSpin = spin(1, 1440, QStringLiteral(" min"), group);
    form->addRow(QStringLiteral("Interval"), m_beaconIntervalSpin);
    m_beaconTextEdit = new QLineEdit(group);
    m_beaconTextEdit->setToolTip(QStringLiteral("Placeholders: {call}, {name}, {loc}"));
    form->addRow(QStringLiteral("Text"), m_beaconTextEdit);
    m_beaconOnConnectCheck = new QCheckBox(QStringLiteral("Send one when the modem starts"), group);
    form->addRow(QString(), m_beaconOnConnectCheck);
    m_beaconDestEdit = new QLineEdit(group);
    m_beaconDestEdit->setPlaceholderText(QStringLiteral("blank = station destination"));
    form->addRow(QStringLiteral("Destination"), m_beaconDestEdit);
    m_beaconDigiEdit = new QLineEdit(group);
    m_beaconDigiEdit->setPlaceholderText(QStringLiteral("blank = station path"));
    form->addRow(QStringLiteral("Digipeater path"), m_beaconDigiEdit);
    return group;
}

QWidget *SettingsPage::buildPositionGroup()
{
    auto *group = new QGroupBox(QStringLiteral("APRS position"), this);
    auto *grid = new QGridLayout(group);

    auto *left = new QFormLayout;
    m_posEnabledCheck = new QCheckBox(QStringLiteral("Send the position periodically"), group);
    left->addRow(QString(), m_posEnabledCheck);
    m_posIntervalSpin = spin(1, 1440, QStringLiteral(" min"), group);
    left->addRow(QStringLiteral("Interval"), m_posIntervalSpin);
    m_posSourceCombo = new QComboBox(group);
    m_posSourceCombo->addItem(QStringLiteral("Fixed coordinates"), QStringLiteral("fixed"));
    m_posSourceCombo->addItem(QStringLiteral("GPS receiver"), QStringLiteral("gps"));
    left->addRow(QStringLiteral("Source"), m_posSourceCombo);
    m_latSpin = dspin(-90.0, 90.0, 5, QString(), group);
    left->addRow(QStringLiteral("Latitude"), m_latSpin);
    m_lonSpin = dspin(-180.0, 180.0, 5, QString(), group);
    left->addRow(QStringLiteral("Longitude"), m_lonSpin);
    m_altSpin = dspin(-500.0, 20000.0, 0, QStringLiteral(" m"), group);
    left->addRow(QStringLiteral("Altitude"), m_altSpin);
    auto *fill = new QPushButton(QStringLiteral("Fill from locator"), group);
    fill->setToolTip(QStringLiteral("Centre of the Maidenhead square entered in the Station section"));
    connect(fill, &QPushButton::clicked, this, &SettingsPage::fillFromLocator);
    left->addRow(QString(), fill);

    auto *gpsRow = new QHBoxLayout;
    m_gpsPortCombo = new QComboBox(group);
    m_gpsPortCombo->setEditable(true);
    gpsRow->addWidget(m_gpsPortCombo, 1);
    auto *refresh = new QPushButton(QStringLiteral("Refresh"), group);
    connect(refresh, &QPushButton::clicked, this, &SettingsPage::refreshGpsPorts);
    gpsRow->addWidget(refresh);
    left->addRow(QStringLiteral("GPS serial port"), gpsRow);
    m_gpsBaudCombo = new QComboBox(group);
    for (const int baud : gps::CommonBaudRates) m_gpsBaudCombo->addItem(QString::number(baud), baud);
    left->addRow(QStringLiteral("Baud rate"), m_gpsBaudCombo);
    m_gpsMaxAgeSpin = spin(1, 3600, QStringLiteral(" s"), group);
    left->addRow(QStringLiteral("Maximum fix age"), m_gpsMaxAgeSpin);
    m_gpsCourseCheck = new QCheckBox(QStringLiteral("Send course and speed"), group);
    left->addRow(QString(), m_gpsCourseCheck);
    m_gpsAutoOpenCheck = new QCheckBox(QStringLiteral("Open the GPS port at start-up"), group);
    left->addRow(QString(), m_gpsAutoOpenCheck);
    grid->addLayout(left, 0, 0);

    auto *right = new QFormLayout;
    auto *symbolRow = new QHBoxLayout;
    m_symbolTableEdit = new QLineEdit(group);
    m_symbolTableEdit->setMaxLength(1);
    m_symbolTableEdit->setFixedWidth(40);
    m_symbolCodeEdit = new QLineEdit(group);
    m_symbolCodeEdit->setMaxLength(1);
    m_symbolCodeEdit->setFixedWidth(40);
    m_symbolLabel = new QLabel(group);
    connect(m_symbolTableEdit, &QLineEdit::textChanged, this, &SettingsPage::updateSymbolDisplay);
    connect(m_symbolCodeEdit, &QLineEdit::textChanged, this, &SettingsPage::updateSymbolDisplay);
    symbolRow->addWidget(m_symbolTableEdit);
    symbolRow->addWidget(m_symbolCodeEdit);
    symbolRow->addWidget(m_symbolLabel, 1);
    auto *choose = new QPushButton(QStringLiteral("Choose..."), group);
    connect(choose, &QPushButton::clicked, this, &SettingsPage::chooseSymbol);
    symbolRow->addWidget(choose);
    right->addRow(QStringLiteral("Map symbol"), symbolRow);
    m_posCommentEdit = new QLineEdit(group);
    m_posCommentEdit->setMaxLength(aprs::MaxCommentLen);
    right->addRow(QStringLiteral("Comment"), m_posCommentEdit);
    m_sendAltitudeCheck = new QCheckBox(QStringLiteral("Send the altitude"), group);
    right->addRow(QString(), m_sendAltitudeCheck);
    m_messagingCapableCheck = new QCheckBox(QStringLiteral("Announce messaging capability ('=' rather than '!')"), group);
    right->addRow(QString(), m_messagingCapableCheck);
    m_posDestEdit = new QLineEdit(group);
    m_posDestEdit->setToolTip(QStringLiteral("A software identifier, not a real station. APZ... is reserved for experimental software."));
    right->addRow(QStringLiteral("APRS destination"), m_posDestEdit);
    m_posDigiEdit = new QLineEdit(group);
    m_posDigiEdit->setPlaceholderText(QStringLiteral("blank = station path"));
    right->addRow(QStringLiteral("Digipeater path"), m_posDigiEdit);
    m_posOnConnectCheck = new QCheckBox(QStringLiteral("Send one when the modem starts"), group);
    right->addRow(QString(), m_posOnConnectCheck);

    auto *sb = new QGroupBox(QStringLiteral("SmartBeaconing (GPS source only)"), group);
    auto *sbForm = new QFormLayout(sb);
    m_sbCheck = new QCheckBox(QStringLiteral("Replace the fixed interval with one derived from speed and turns"), sb);
    sbForm->addRow(QString(), m_sbCheck);
    m_sbLowSpin = dspin(0.0, 500.0, 1, QStringLiteral(" km/h"), sb);
    sbForm->addRow(QStringLiteral("Stopped below"), m_sbLowSpin);
    m_sbHighSpin = dspin(1.0, 500.0, 1, QStringLiteral(" km/h"), sb);
    sbForm->addRow(QStringLiteral("Fast above"), m_sbHighSpin);
    m_sbSlowSpin = spin(10, 86400, QStringLiteral(" s"), sb);
    sbForm->addRow(QStringLiteral("Slow interval"), m_sbSlowSpin);
    m_sbFastSpin = spin(5, 86400, QStringLiteral(" s"), sb);
    sbForm->addRow(QStringLiteral("Fast interval"), m_sbFastSpin);
    m_sbTurnMinSpin = dspin(1.0, 180.0, 1, QStringLiteral(" deg"), sb);
    sbForm->addRow(QStringLiteral("Turn threshold"), m_sbTurnMinSpin);
    m_sbTurnSlopeSpin = dspin(0.0, 500.0, 1, QString(), sb);
    sbForm->addRow(QStringLiteral("Turn slope"), m_sbTurnSlopeSpin);
    m_sbTurnTimeSpin = spin(0, 3600, QStringLiteral(" s"), sb);
    sbForm->addRow(QStringLiteral("Minimum time between turns"), m_sbTurnTimeSpin);
    m_sbPreviewLabel = hint(QString(), sb);
    sbForm->addRow(m_sbPreviewLabel);
    for (auto *w : {m_sbLowSpin, m_sbHighSpin}) connect(w, &QDoubleSpinBox::valueChanged, this, &SettingsPage::updateSmartPreview);
    for (auto *w : {m_sbSlowSpin, m_sbFastSpin}) connect(w, &QSpinBox::valueChanged, this, &SettingsPage::updateSmartPreview);
    right->addRow(sb);
    grid->addLayout(right, 0, 1);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    return group;
}

QWidget *SettingsPage::buildMessagingGroup()
{
    auto *group = new QGroupBox(QStringLiteral("APRS messaging"), this);
    auto *form = new QFormLayout(group);
    m_autoAckCheck = new QCheckBox(QStringLiteral("Acknowledge messages addressed to me"), group);
    form->addRow(QString(), m_autoAckCheck);
    m_maxAttemptsSpin = spin(1, 20, QString(), group);
    form->addRow(QStringLiteral("Transmissions before giving up"), m_maxAttemptsSpin);
    m_retrySecondsSpin = spin(5, 600, QStringLiteral(" s"), group);
    form->addRow(QStringLiteral("First retry after"), m_retrySecondsSpin);
    m_retryPreviewLabel = hint(QString(), group);
    form->addRow(m_retryPreviewLabel);
    connect(m_maxAttemptsSpin, &QSpinBox::valueChanged, this, &SettingsPage::updateRetryPreview);
    connect(m_retrySecondsSpin, &QSpinBox::valueChanged, this, &SettingsPage::updateRetryPreview);
    m_trackStationsCheck = new QCheckBox(QStringLiteral("Remember positions of stations heard"), group);
    form->addRow(QString(), m_trackStationsCheck);
    m_stationExpirySpin = spin(0, 100000, QStringLiteral(" min"), group);
    m_stationExpirySpin->setSpecialValueText(QStringLiteral("never"));
    form->addRow(QStringLiteral("Forget a station after"), m_stationExpirySpin);
    return group;
}

QWidget *SettingsPage::buildNotificationGroup()
{
    auto *group = new QGroupBox(QStringLiteral("Notifications"), this);
    auto *form = new QFormLayout(group);
    m_notifyEnabledCheck = new QCheckBox(QStringLiteral("Enable notifications"), group);
    form->addRow(QString(), m_notifyEnabledCheck);
    m_notifySoundCheck = new QCheckBox(QStringLiteral("Sound"), group);
    form->addRow(QString(), m_notifySoundCheck);
    m_notifyFlashCheck = new QCheckBox(QStringLiteral("Flash the window when it is not in front"), group);
    form->addRow(QString(), m_notifyFlashCheck);
    auto *soundRow = new QHBoxLayout;
    m_soundFileEdit = new QLineEdit(group);
    m_soundFileEdit->setPlaceholderText(QStringLiteral("blank = the built-in chime"));
    soundRow->addWidget(m_soundFileEdit, 1);
    auto *browse = new QPushButton(QStringLiteral("Browse..."), group);
    connect(browse, &QPushButton::clicked, this, &SettingsPage::browseSound);
    soundRow->addWidget(browse);
    auto *test = new QPushButton(QStringLiteral("Test"), group);
    connect(test, &QPushButton::clicked, this, &SettingsPage::testSoundRequested);
    soundRow->addWidget(test);
    form->addRow(QStringLiteral("Sound file"), soundRow);
    m_volumeSpin = spin(0, 100, QStringLiteral(" %"), group);
    form->addRow(QStringLiteral("Volume"), m_volumeSpin);
    m_minIntervalSpin = spin(0, 600, QStringLiteral(" s"), group);
    form->addRow(QStringLiteral("At most one alert per category every"), m_minIntervalSpin);
    m_onMessageCheck = new QCheckBox(QStringLiteral("APRS message addressed to me"), group);
    m_onChatCheck = new QCheckBox(QStringLiteral("Chat text on the unproto destination"), group);
    m_onBulletinCheck = new QCheckBox(QStringLiteral("Bulletin or announcement"), group);
    m_onWeatherCheck = new QCheckBox(QStringLiteral("Weather bulletin"), group);
    m_onPositionCheck = new QCheckBox(QStringLiteral("Position report from another station"), group);
    m_onStatusCheck = new QCheckBox(QStringLiteral("Status report"), group);
    for (auto *c : {m_onMessageCheck, m_onChatCheck, m_onBulletinCheck, m_onWeatherCheck, m_onPositionCheck, m_onStatusCheck}) {
        form->addRow(QString(), c);
    }
    return group;
}

QWidget *SettingsPage::buildAprsIsGroup()
{
    auto *group = new QGroupBox(QStringLiteral("APRS Internet System (aprs.fi)"), this);
    auto *form = new QFormLayout(group);
    m_isEnabledCheck = new QCheckBox(QStringLiteral("Connect to the APRS Internet System"), group);
    m_isEnabledCheck->setToolTip(QStringLiteral(
        "What is heard on the air is sent to the network, where aprs.fi and every other client can see it, "
        "and the traffic around your position comes back into the stations list."));
    form->addRow(QString(), m_isEnabledCheck);

    m_isRegionCombo = new QComboBox(group);
    m_isRegionCombo->addItem(QStringLiteral("Any (rotate.aprs2.net)"), QStringLiteral("rotate"));
    m_isRegionCombo->addItem(QStringLiteral("Europe and Africa (euro.aprs2.net)"), QStringLiteral("euro"));
    m_isRegionCombo->addItem(QStringLiteral("North America (noam.aprs2.net)"), QStringLiteral("noam"));
    m_isRegionCombo->addItem(QStringLiteral("South America (soam.aprs2.net)"), QStringLiteral("soam"));
    m_isRegionCombo->addItem(QStringLiteral("Asia (asia.aprs2.net)"), QStringLiteral("asia"));
    m_isRegionCombo->addItem(QStringLiteral("Oceania (aunz.aprs2.net)"), QStringLiteral("aunz"));
    m_isRegionCombo->addItem(QStringLiteral("Another server"), QStringLiteral("custom"));
    m_isRegionCombo->setToolTip(QStringLiteral("The Tier 2 network's regional addresses; pick the one for your region."));
    form->addRow(QStringLiteral("Server"), m_isRegionCombo);

    auto *hostRow = new QHBoxLayout;
    m_isHostEdit = new QLineEdit(group);
    m_isHostEdit->setPlaceholderText(QStringLiteral("host name, for Another server"));
    hostRow->addWidget(m_isHostEdit, 1);
    m_isPortSpin = new QSpinBox(group);
    m_isPortSpin->setRange(1, 65535);
    m_isPortSpin->setValue(14580);
    m_isPortSpin->setToolTip(QStringLiteral("14580 is the client-defined filter port, the one to use."));
    hostRow->addWidget(m_isPortSpin);
    form->addRow(QStringLiteral("Host, port"), hostRow);
    connect(m_isRegionCombo, &QComboBox::currentIndexChanged, this, [this] {
        m_isHostEdit->setEnabled(m_isRegionCombo->currentData().toString() == QLatin1String("custom"));
    });

    auto *passRow = new QHBoxLayout;
    m_isPasscodeEdit = new QLineEdit(group);
    m_isPasscodeEdit->setPlaceholderText(QStringLiteral("for your callsign; empty = download only"));
    m_isPasscodeEdit->setValidator(new QRegularExpressionValidator(QRegularExpression(QStringLiteral("-?[0-9]{0,5}")), m_isPasscodeEdit));
    m_isPasscodeEdit->setToolTip(QStringLiteral(
        "Derived from your callsign (without SSID). Without it the connection is accepted but nothing "
        "you upload is; the traffic around you still arrives."));
    passRow->addWidget(m_isPasscodeEdit, 1);
    auto *computeButton = new QPushButton(QStringLiteral("Fill from callsign"), group);
    computeButton->setToolTip(QStringLiteral("Compute the passcode for the callsign entered in Station."));
    connect(computeButton, &QPushButton::clicked, this, [this] {
        const int code = aprs::passcode(m_callsignEdit->text());
        if (code < 0) {
            QMessageBox::warning(this, QStringLiteral("Passcode"), QStringLiteral("Enter your callsign first, in Station."));
            return;
        }
        m_isPasscodeEdit->setText(QString::number(code));
    });
    passRow->addWidget(computeButton);
    form->addRow(QStringLiteral("Passcode"), passRow);

    m_isGateCheck = new QCheckBox(QStringLiteral("Upload what is heard on the air (receive-only IGate)"), group);
    m_isGateCheck->setToolTip(QStringLiteral(
        "Packets heard on the air that decode as APRS are sent with the qAO construct and your callsign, as a "
        "receive-only IGate does. Chat traffic is not APRS and is never sent. Needs the passcode."));
    form->addRow(QString(), m_isGateCheck);

    auto *radiusRow = new QHBoxLayout;
    m_isReceiveCheck = new QCheckBox(QStringLiteral("Download the traffic within"), group);
    radiusRow->addWidget(m_isReceiveCheck);
    m_isRadiusSpin = new QSpinBox(group);
    m_isRadiusSpin->setRange(1, 500);
    m_isRadiusSpin->setSuffix(QStringLiteral(" km"));
    m_isRadiusSpin->setValue(50);
    m_isRadiusSpin->setToolTip(QStringLiteral("A range filter around your position (GPS fix, or the fixed coordinates), 1 to 500 km."));
    radiusRow->addWidget(m_isRadiusSpin);
    radiusRow->addStretch(1);
    form->addRow(QString(), radiusRow);

    m_isShowCheck = new QCheckBox(QStringLiteral("Show the Internet traffic in the chat too (not only in the stations list)"), group);
    form->addRow(QString(), m_isShowCheck);

    auto *positionRow = new QHBoxLayout;
    m_isPositionCheck = new QCheckBox(QStringLiteral("Upload my position (on the schedule of the APRS position section)"), group);
    m_isPositionCheck->setToolTip(QStringLiteral(
        "The same report as on the air, sent straight to the network with the TCPIP* path, at the periodic "
        "interval of the APRS position section or by SmartBeaconing. Needs the passcode."));
    positionRow->addWidget(m_isPositionCheck);
    m_isPositionOnConnectCheck = new QCheckBox(QStringLiteral("and right after the login"), group);
    positionRow->addWidget(m_isPositionOnConnectCheck);
    positionRow->addStretch(1);
    form->addRow(QString(), positionRow);
    return group;
}

QWidget *SettingsPage::buildInterfaceGroup()
{
    auto *group = new QGroupBox(QStringLiteral("Interface"), this);
    auto *form = new QFormLayout(group);
    m_styleCombo = new QComboBox(group);
    m_styleCombo->addItem(QStringLiteral("Platform default"), QString());
    const QStringList styles = QStyleFactory::keys();
    for (const QString &s : styles) m_styleCombo->addItem(s, s);
    m_styleCombo->setToolTip(QStringLiteral("Applied at the next start; Qt cannot restyle live widgets reliably"));
    form->addRow(QStringLiteral("Qt style"), m_styleCombo);
    m_timestampsCheck = new QCheckBox(QStringLiteral("Show timestamps"), group);
    form->addRow(QString(), m_timestampsCheck);
    m_monitorCheck = new QCheckBox(QStringLiteral("Show traffic not addressed to the chat (monitor)"), group);
    form->addRow(QString(), m_monitorCheck);
    m_fontSizeSpin = spin(6, 32, QStringLiteral(" pt"), group);
    form->addRow(QStringLiteral("Conversation font size"), m_fontSizeSpin);
    m_maxLinesSpin = spin(100, 100000, QStringLiteral(" lines"), group);
    form->addRow(QStringLiteral("Lines kept in the window"), m_maxLinesSpin);

    // The map: tile servers that need no key, the cache on disk.
    m_mapProviderCombo = new QComboBox(group);
    for (const tiles::Provider &provider : tiles::providers()) m_mapProviderCombo->addItem(provider.name, provider.key);
    m_mapProviderCombo->addItem(QStringLiteral("Another tile server"), QStringLiteral("custom"));
    m_mapProviderCombo->setToolTip(QStringLiteral(
        "Tile servers that need no key or account. IGN's Plan and orthophotos cover France."));
    form->addRow(QStringLiteral("Map tiles"), m_mapProviderCombo);
    m_mapUrlEdit = new QLineEdit(group);
    m_mapUrlEdit->setPlaceholderText(QStringLiteral("https://host/{z}/{x}/{y}.png, for Another tile server"));
    form->addRow(QStringLiteral("Tile URL"), m_mapUrlEdit);
    connect(m_mapProviderCombo, &QComboBox::currentIndexChanged, this, [this] {
        m_mapUrlEdit->setEnabled(m_mapProviderCombo->currentData().toString() == QLatin1String("custom"));
    });
    m_mapSharpCheck = new QCheckBox(QStringLiteral("Sharp tiles on a high-density screen (one zoom level deeper, drawn at half size)"), group);
    form->addRow(QString(), m_mapSharpCheck);
    auto *cacheRow = new QHBoxLayout;
    m_mapCacheSpin = spin(20, 5000, QStringLiteral(" MB"), group);
    m_mapCacheSpin->setToolTip(QStringLiteral("Tiles are kept on disk once downloaded, so an area seen once stays available offline."));
    cacheRow->addWidget(m_mapCacheSpin);
    m_mapCacheLabel = hint(QString(), group);
    cacheRow->addWidget(m_mapCacheLabel, 1);
    auto *clearTiles = new QPushButton(QStringLiteral("Clear tiles"), group);
    connect(clearTiles, &QPushButton::clicked, this, [this] {
        CachingNetworkAccessManager::clear();
        m_mapCacheLabel->setText(QStringLiteral("On disk: 0.0 MB"));
    });
    cacheRow->addWidget(clearTiles);
    form->addRow(QStringLiteral("Tile cache"), cacheRow);
    return group;
}


// ---- load / collect --------------------------------------------------------------------

void SettingsPage::load(const AppConfig &c)
{
    m_config = c;
    m_callsignEdit->setText(c.station.callsign);
    m_ssidSpin->setValue(c.station.ssid);
    m_destinationEdit->setText(c.station.destination);
    m_digiEdit->setText(c.station.digipeaters);
    m_nameEdit->setText(c.station.operatorName);
    m_locatorEdit->setText(c.station.locator);
    m_encodingCombo->setCurrentIndex(qMax(0, m_encodingCombo->findData(c.station.textEncoding)));
    m_maxInfoSpin->setValue(c.station.maxInfoLen);

    m_modemConfigEdit->setText(c.modem.configFile.isEmpty() ? modemconf::defaultFile() : c.modem.configFile);
    m_modemChannelSpin->setValue(c.modem.channel);
    m_modemAutoStartCheck->setChecked(c.modem.autoStart);
    m_modemEchoCheck->setChecked(c.modem.echoLog);
    m_modemEnabledCheck->setChecked(c.modem.enabled);
    m_sendParamsCheck->setChecked(c.modem.sendParams);
    m_txdelaySpin->setValue(c.modem.txdelay);
    m_persistSpin->setValue(c.modem.persistence);
    m_slottimeSpin->setValue(c.modem.slottime);
    m_txtailSpin->setValue(c.modem.txtail);
    m_fullDuplexCheck->setChecked(c.modem.fullDuplex);

    m_useDcdCheck->setChecked(c.channel.useDcd);
    m_holdOffSpin->setValue(c.channel.rxHoldOffMs);
    m_gapSpin->setValue(c.channel.interFrameGapMs);
    m_jitterSpin->setValue(c.channel.randomJitterMs);
    m_maxDeferSpin->setValue(c.channel.maxDeferSeconds);
    m_waitTxbufCheck->setChecked(c.channel.waitForTxbufEmpty);
    m_ownEchoCheck->setChecked(c.channel.treatOwnEchoAsBusy);

    m_beaconCheck->setChecked(c.beacon.enabled);
    m_beaconIntervalSpin->setValue(c.beacon.intervalMinutes);
    m_beaconTextEdit->setText(c.beacon.text);
    m_beaconOnConnectCheck->setChecked(c.beacon.sendOnConnect);
    m_beaconDestEdit->setText(c.beacon.destination);
    m_beaconDigiEdit->setText(c.beacon.digipeaters);

    m_posEnabledCheck->setChecked(c.position.enabled);
    m_posIntervalSpin->setValue(c.position.intervalMinutes);
    m_posSourceCombo->setCurrentIndex(qMax(0, m_posSourceCombo->findData(c.position.source)));
    m_latSpin->setValue(c.position.latitude);
    m_lonSpin->setValue(c.position.longitude);
    m_altSpin->setValue(c.position.altitudeM);
    refreshGpsPorts();
    if (!c.position.gpsPort.isEmpty()) {
        const int index = m_gpsPortCombo->findData(c.position.gpsPort);
        if (index >= 0) m_gpsPortCombo->setCurrentIndex(index);
        else m_gpsPortCombo->setEditText(c.position.gpsPort);
    }
    m_gpsBaudCombo->setCurrentIndex(qMax(0, m_gpsBaudCombo->findData(c.position.gpsBaud)));
    m_gpsMaxAgeSpin->setValue(c.position.gpsMaxFixAgeS);
    m_gpsCourseCheck->setChecked(c.position.gpsSendCourseSpeed);
    m_gpsAutoOpenCheck->setChecked(c.position.gpsAutoOpen);
    m_sbCheck->setChecked(c.position.smartEnabled);
    m_sbLowSpin->setValue(c.position.smartLowSpeedKmh);
    m_sbHighSpin->setValue(c.position.smartHighSpeedKmh);
    m_sbSlowSpin->setValue(c.position.smartSlowIntervalS);
    m_sbFastSpin->setValue(c.position.smartFastIntervalS);
    m_sbTurnMinSpin->setValue(c.position.smartTurnMinDeg);
    m_sbTurnSlopeSpin->setValue(c.position.smartTurnSlope);
    m_sbTurnTimeSpin->setValue(c.position.smartTurnTimeS);
    m_symbolTableEdit->setText(c.position.symbolTable);
    m_symbolCodeEdit->setText(c.position.symbolCode);
    m_posCommentEdit->setText(c.position.comment);
    m_sendAltitudeCheck->setChecked(c.position.sendAltitude);
    m_messagingCapableCheck->setChecked(c.position.messagingCapable);
    m_posDestEdit->setText(c.position.destination);
    m_posDigiEdit->setText(c.position.digipeaters);
    m_posOnConnectCheck->setChecked(c.position.sendOnConnect);

    m_autoAckCheck->setChecked(c.messaging.autoAck);
    m_maxAttemptsSpin->setValue(c.messaging.maxAttempts);
    m_retrySecondsSpin->setValue(c.messaging.retrySeconds);
    m_trackStationsCheck->setChecked(c.messaging.trackStations);
    m_stationExpirySpin->setValue(c.messaging.stationExpiryMinutes);

    m_isEnabledCheck->setChecked(c.aprsis.enabled);
    m_isRegionCombo->setCurrentIndex(std::max(0, m_isRegionCombo->findData(c.aprsis.region)));
    m_isHostEdit->setText(c.aprsis.customHost);
    m_isHostEdit->setEnabled(c.aprsis.region == QLatin1String("custom"));
    m_isPortSpin->setValue(c.aprsis.port);
    m_isPasscodeEdit->setText(c.aprsis.passcode < 0 ? QString() : QString::number(c.aprsis.passcode));
    m_isGateCheck->setChecked(c.aprsis.gateRf);
    m_isReceiveCheck->setChecked(c.aprsis.receive);
    m_isRadiusSpin->setValue(c.aprsis.radiusKm);
    m_isShowCheck->setChecked(c.aprsis.showInChat);
    m_isPositionCheck->setChecked(c.aprsis.positionEnabled);
    m_isPositionOnConnectCheck->setChecked(c.aprsis.positionOnConnect);

    m_mapProviderCombo->setCurrentIndex(std::max(0, m_mapProviderCombo->findData(c.ui.mapProvider)));
    m_mapUrlEdit->setText(c.ui.mapCustomUrl);
    m_mapUrlEdit->setEnabled(c.ui.mapProvider == QLatin1String("custom"));
    m_mapSharpCheck->setChecked(c.ui.mapSharp);
    m_mapCacheSpin->setValue(c.ui.mapCacheMb);
    m_mapCacheLabel->setText(QStringLiteral("On disk: %1 MB").arg(CachingNetworkAccessManager::sizeOnDisk() / (1024.0 * 1024.0), 0, 'f', 1));

    m_notifyEnabledCheck->setChecked(c.notifications.enabled);
    m_notifySoundCheck->setChecked(c.notifications.sound);
    m_notifyFlashCheck->setChecked(c.notifications.flashWindow);
    m_soundFileEdit->setText(c.notifications.soundFile);
    m_volumeSpin->setValue(c.notifications.volume);
    m_minIntervalSpin->setValue(c.notifications.minIntervalS);
    m_onMessageCheck->setChecked(c.notifications.onMessage);
    m_onChatCheck->setChecked(c.notifications.onChat);
    m_onBulletinCheck->setChecked(c.notifications.onBulletin);
    m_onWeatherCheck->setChecked(c.notifications.onWeather);
    m_onPositionCheck->setChecked(c.notifications.onPosition);
    m_onStatusCheck->setChecked(c.notifications.onStatus);

    m_styleCombo->setCurrentIndex(qMax(0, m_styleCombo->findData(c.ui.qtStyle)));
    m_timestampsCheck->setChecked(c.ui.showTimestamps);
    m_monitorCheck->setChecked(c.ui.showMonitor);
    m_fontSizeSpin->setValue(c.ui.fontSize);
    m_maxLinesSpin->setValue(c.ui.maxLogLines);

    updateSymbolDisplay();
    updateSmartPreview();
    updateRetryPreview();
}

AppConfig SettingsPage::collect() const
{
    AppConfig c = m_config;
    c.station.callsign = m_callsignEdit->text().trimmed().toUpper();
    c.station.ssid = m_ssidSpin->value();
    c.station.destination = m_destinationEdit->text().trimmed().toUpper().isEmpty() ? QStringLiteral("CHAT")
                                                                                     : m_destinationEdit->text().trimmed().toUpper();
    c.station.digipeaters = m_digiEdit->text().trimmed().toUpper();
    c.station.operatorName = m_nameEdit->text().trimmed();
    c.station.locator = m_locatorEdit->text().trimmed().toUpper();
    c.station.textEncoding = m_encodingCombo->currentData().toString();
    c.station.maxInfoLen = m_maxInfoSpin->value();

    c.modem.configFile = m_modemConfigEdit->text().trimmed();
    c.modem.channel = m_modemChannelSpin->value();
    c.modem.autoStart = m_modemAutoStartCheck->isChecked();
    c.modem.echoLog = m_modemEchoCheck->isChecked();
    c.modem.enabled = m_modemEnabledCheck->isChecked();
    c.modem.sendParams = m_sendParamsCheck->isChecked();
    c.modem.txdelay = m_txdelaySpin->value();
    c.modem.persistence = m_persistSpin->value();
    c.modem.slottime = m_slottimeSpin->value();
    c.modem.txtail = m_txtailSpin->value();
    c.modem.fullDuplex = m_fullDuplexCheck->isChecked();

    c.channel.useDcd = m_useDcdCheck->isChecked();
    c.channel.rxHoldOffMs = m_holdOffSpin->value();
    c.channel.interFrameGapMs = m_gapSpin->value();
    c.channel.randomJitterMs = m_jitterSpin->value();
    c.channel.maxDeferSeconds = m_maxDeferSpin->value();
    c.channel.waitForTxbufEmpty = m_waitTxbufCheck->isChecked();
    c.channel.treatOwnEchoAsBusy = m_ownEchoCheck->isChecked();

    c.beacon.enabled = m_beaconCheck->isChecked();
    c.beacon.intervalMinutes = m_beaconIntervalSpin->value();
    c.beacon.text = m_beaconTextEdit->text();
    c.beacon.sendOnConnect = m_beaconOnConnectCheck->isChecked();
    c.beacon.destination = m_beaconDestEdit->text().trimmed().toUpper();
    c.beacon.digipeaters = m_beaconDigiEdit->text().trimmed().toUpper();

    c.position.enabled = m_posEnabledCheck->isChecked();
    c.position.intervalMinutes = m_posIntervalSpin->value();
    c.position.source = m_posSourceCombo->currentData().toString();
    c.position.latitude = m_latSpin->value();
    c.position.longitude = m_lonSpin->value();
    c.position.altitudeM = m_altSpin->value();
    c.position.gpsPort = m_gpsPortCombo->currentData().toString().isEmpty() ? m_gpsPortCombo->currentText().trimmed()
                                                                            : m_gpsPortCombo->currentData().toString();
    c.position.gpsBaud = m_gpsBaudCombo->currentData().toInt();
    c.position.gpsMaxFixAgeS = m_gpsMaxAgeSpin->value();
    c.position.gpsSendCourseSpeed = m_gpsCourseCheck->isChecked();
    c.position.gpsAutoOpen = m_gpsAutoOpenCheck->isChecked();
    c.position.smartEnabled = m_sbCheck->isChecked();
    c.position.smartLowSpeedKmh = m_sbLowSpin->value();
    c.position.smartHighSpeedKmh = m_sbHighSpin->value();
    c.position.smartSlowIntervalS = m_sbSlowSpin->value();
    c.position.smartFastIntervalS = m_sbFastSpin->value();
    c.position.smartTurnMinDeg = m_sbTurnMinSpin->value();
    c.position.smartTurnSlope = m_sbTurnSlopeSpin->value();
    c.position.smartTurnTimeS = m_sbTurnTimeSpin->value();
    c.position.symbolTable = m_symbolTableEdit->text();
    c.position.symbolCode = m_symbolCodeEdit->text();
    c.position.comment = m_posCommentEdit->text();
    c.position.sendAltitude = m_sendAltitudeCheck->isChecked();
    c.position.messagingCapable = m_messagingCapableCheck->isChecked();
    c.position.destination = m_posDestEdit->text().trimmed().toUpper();
    c.position.digipeaters = m_posDigiEdit->text().trimmed().toUpper();
    c.position.sendOnConnect = m_posOnConnectCheck->isChecked();

    c.messaging.autoAck = m_autoAckCheck->isChecked();
    c.messaging.maxAttempts = m_maxAttemptsSpin->value();
    c.messaging.retrySeconds = m_retrySecondsSpin->value();
    c.messaging.trackStations = m_trackStationsCheck->isChecked();
    c.messaging.stationExpiryMinutes = m_stationExpirySpin->value();

    c.aprsis.enabled = m_isEnabledCheck->isChecked();
    c.aprsis.region = m_isRegionCombo->currentData().toString();
    c.aprsis.customHost = m_isHostEdit->text().trimmed();
    c.aprsis.port = m_isPortSpin->value();
    c.aprsis.passcode = m_isPasscodeEdit->text().trimmed().isEmpty() ? -1 : m_isPasscodeEdit->text().trimmed().toInt();
    c.aprsis.gateRf = m_isGateCheck->isChecked();
    c.aprsis.receive = m_isReceiveCheck->isChecked();
    c.aprsis.radiusKm = m_isRadiusSpin->value();
    c.aprsis.showInChat = m_isShowCheck->isChecked();
    c.aprsis.positionEnabled = m_isPositionCheck->isChecked();
    c.aprsis.positionOnConnect = m_isPositionOnConnectCheck->isChecked();

    c.ui.mapProvider = m_mapProviderCombo->currentData().toString();
    c.ui.mapCustomUrl = m_mapUrlEdit->text().trimmed();
    c.ui.mapSharp = m_mapSharpCheck->isChecked();
    c.ui.mapCacheMb = m_mapCacheSpin->value();

    c.notifications.enabled = m_notifyEnabledCheck->isChecked();
    c.notifications.sound = m_notifySoundCheck->isChecked();
    c.notifications.flashWindow = m_notifyFlashCheck->isChecked();
    c.notifications.soundFile = m_soundFileEdit->text().trimmed();
    c.notifications.volume = m_volumeSpin->value();
    c.notifications.minIntervalS = m_minIntervalSpin->value();
    c.notifications.onMessage = m_onMessageCheck->isChecked();
    c.notifications.onChat = m_onChatCheck->isChecked();
    c.notifications.onBulletin = m_onBulletinCheck->isChecked();
    c.notifications.onWeather = m_onWeatherCheck->isChecked();
    c.notifications.onPosition = m_onPositionCheck->isChecked();
    c.notifications.onStatus = m_onStatusCheck->isChecked();

    c.ui.qtStyle = m_styleCombo->currentData().toString();
    c.ui.showTimestamps = m_timestampsCheck->isChecked();
    c.ui.showMonitor = m_monitorCheck->isChecked();
    c.ui.fontSize = m_fontSizeSpin->value();
    c.ui.maxLogLines = m_maxLinesSpin->value();
    return c;
}


// ---- validation and saving ---------------------------------------------------------

QString SettingsPage::validate() const
{
    // The GPS port is the one check the model cannot make: it is a field
    // of this page, and only matters with the serial source.
    if (m_posEnabledCheck->isChecked() && m_posSourceCombo->currentData().toString() == QLatin1String("gps")
        && m_gpsPortCombo->currentData().toString().isEmpty() && m_gpsPortCombo->currentText().trimmed().isEmpty()) {
        return QStringLiteral("Periodic position reporting is enabled with the GPS source, but no serial port is selected.");
    }
    return collect().validate();
}

void SettingsPage::onSave()
{
    const QString error = validate();
    if (!error.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Invalid configuration"), error);
        return;
    }
    const AppConfig config = collect();
    QString saveError;
    const QString path = config.save(QString(), &saveError);
    if (path.isEmpty()) {
        QMessageBox::critical(this, QStringLiteral("Save failed"), saveError);
        return;
    }
    m_config = config;
    m_pathLabel->setText(QStringLiteral("Saved to %1").arg(path));
    emit settingsSaved(config);
}


// ---- helpers -----------------------------------------------------------------------------

void SettingsPage::browseModemConfig()
{
    QString start = m_modemConfigEdit->text().trimmed();
    if (start.isEmpty()) start = AppConfig::configDir();
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Select direwolf.conf"), start,
                                                      QStringLiteral("Dire Wolf configuration (*.conf);;All files (*)"));
    if (path.isEmpty()) return;
    m_modemConfigEdit->setText(path);
    onModemConfigChosen(path);
}

// Choosing a file reads its CHANNEL lines and offers to align the setting.
void SettingsPage::onModemConfigChosen(const QString &path)
{
    if (path.trimmed().isEmpty() || !QFileInfo(path).isFile()) return;
    const QList<int> channels = modemconf::channelsIn(path);
    if (channels.isEmpty() || channels.contains(m_modemChannelSpin->value())) return;
    QStringList names;
    for (const int c : channels) names << QString::number(c);
    const auto answer = QMessageBox::question(
        this, QStringLiteral("Channel mismatch"),
        QStringLiteral("This file declares channel %1, but the radio channel here is %2. Use channel %3?")
            .arg(names.join(QStringLiteral(", "))).arg(m_modemChannelSpin->value()).arg(channels.first()));
    if (answer == QMessageBox::Yes) m_modemChannelSpin->setValue(channels.first());
}

void SettingsPage::createModemConfig()
{
    QString path = m_modemConfigEdit->text().trimmed();
    if (path.isEmpty()) path = modemconf::userPath();
    if (QFileInfo(path).isFile()) {
        QMessageBox::information(this, QStringLiteral("Already there"),
                                 QStringLiteral("%1 already exists and was left alone.").arg(path));
        return;
    }
    QString error;
    const QString written = modemconf::writeStarter(path, m_callsignEdit->text().trimmed().toUpper(), &error);
    if (written.isEmpty()) {
        QMessageBox::critical(this, QStringLiteral("Could not write"), error);
        return;
    }
    m_modemConfigEdit->setText(written);
    QMessageBox::information(this, QStringLiteral("Configuration written"),
                             QStringLiteral("A starter direwolf.conf was written to %1.\n\nEdit it for your audio device "
                                            "and PTT wiring, then save the configuration and start the modem.").arg(written));
}

void SettingsPage::editModemConfig()
{
    const QString path = m_modemConfigEdit->text().trimmed();
    if (path.isEmpty() || !QFileInfo(path).isFile()) {
        QMessageBox::information(this, QStringLiteral("No file"), QStringLiteral("Choose or create a direwolf.conf first."));
        return;
    }
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(path))) {
        QMessageBox::information(this, QStringLiteral("Could not open"),
                                 QStringLiteral("No application is registered for this file. Open it by hand: %1").arg(path));
    }
}

void SettingsPage::refreshGpsPorts()
{
    const QString current = m_gpsPortCombo->currentData().toString().isEmpty() ? m_gpsPortCombo->currentText()
                                                                               : m_gpsPortCombo->currentData().toString();
    m_gpsPortCombo->clear();
    const auto ports = gps::availablePorts();
    for (const auto &p : ports) m_gpsPortCombo->addItem(QStringLiteral("%1  (%2)").arg(p.first, p.second), p.first);
    if (!current.isEmpty()) {
        const int index = m_gpsPortCombo->findData(current);
        if (index >= 0) m_gpsPortCombo->setCurrentIndex(index);
        else m_gpsPortCombo->setEditText(current);
    }
}

void SettingsPage::fillFromLocator()
{
    double lat, lon;
    QString error;
    if (!aprs::locatorToLatLon(m_locatorEdit->text(), lat, lon, &error)) {
        QMessageBox::warning(this, QStringLiteral("Locator"), error);
        return;
    }
    m_latSpin->setValue(lat);
    m_lonSpin->setValue(lon);
}

void SettingsPage::chooseSymbol()
{
    SymbolPicker picker(m_symbolTableEdit->text(), m_symbolCodeEdit->text(), this);
    if (picker.exec() == QDialog::Accepted) {
        m_symbolTableEdit->setText(picker.table());
        m_symbolCodeEdit->setText(picker.code());
    }
}

void SettingsPage::updateSymbolDisplay()
{
    const QString table = m_symbolTableEdit->text();
    const QString code = m_symbolCodeEdit->text();
    QString error;
    if (!aprs::validateSymbol(table, code, &error)) {
        m_symbolLabel->setPixmap(QPixmap());
        m_symbolLabel->setText(error);
        return;
    }
    const QPixmap art = symbolart::pixmap(table, code, 24);
    if (!art.isNull()) {
        m_symbolLabel->setPixmap(art);
        m_symbolLabel->setToolTip(SymbolPicker::describe(table, code));
    } else {
        m_symbolLabel->setPixmap(QPixmap());
        m_symbolLabel->setText(SymbolPicker::describe(table, code));
    }
}

void SettingsPage::updateSmartPreview()
{
    SmartBeaconSettings s;
    s.lowSpeedKmh = m_sbLowSpin->value();
    s.highSpeedKmh = m_sbHighSpin->value();
    s.slowIntervalS = m_sbSlowSpin->value();
    s.fastIntervalS = m_sbFastSpin->value();
    m_sbPreviewLabel->setText(SmartBeaconer(s).preview());
}

// Attempts at 0, 30, 90 s ... with the base interval doubling, then the
// base interval once more as the acknowledgement window.
void SettingsPage::updateRetryPreview()
{
    const int attempts = m_maxAttemptsSpin->value();
    const int base = qMax(5, m_retrySecondsSpin->value());
    QStringList times;
    int at = 0;
    for (int i = 1; i <= attempts; i++) {
        times << QStringLiteral("%1m%2").arg(at / 60).arg(at % 60, 2, 10, QChar('0'));
        at += (i >= attempts) ? base : base * (1 << (i - 1));
    }
    m_retryPreviewLabel->setText(QStringLiteral("Transmissions at %1, reported as failed at %2m%3")
                                     .arg(times.join(QStringLiteral(", "))).arg(at / 60).arg(at % 60, 2, 10, QChar('0')));
}

void SettingsPage::browseSound()
{
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Notification sound"), QString(),
                                                      QStringLiteral("WAV files (*.wav);;All files (*)"));
    if (!path.isEmpty()) m_soundFileEdit->setText(path);
}
