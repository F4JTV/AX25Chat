/*
 * SettingsPage.h - the Configuration tab.
 *
 * Port of ax25chat/ui/settings_page.py.  One group per configuration
 * section.  The Direwolf group of the Python version (executable, arguments,
 * working directory, timeouts, console) is replaced by a Modem group: the
 * direwolf.conf to load, the channel, and the channel access parameters.
 *
 * This file is part of AX25Chat.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "AppConfig.h"

#include <QWidget>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QSpinBox;

class SettingsPage : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsPage(const AppConfig &config, QWidget *parent = nullptr);

    // The page's current contents as a configuration.
    AppConfig collect() const;
    void load(const AppConfig &config);

signals:
    void settingsSaved(const AppConfig &config);
    void testSoundRequested();

private:
    QWidget *buildStationGroup();
    QWidget *buildModemGroup();
    QWidget *buildChannelGroup();
    QWidget *buildBeaconGroup();
    QWidget *buildPositionGroup();
    QWidget *buildMessagingGroup();
    QWidget *buildNotificationGroup();
    QWidget *buildInterfaceGroup();

    QString validate() const;
    void onSave();
    void browseModemConfig();
    void createModemConfig();
    void editModemConfig();
    void onModemConfigChosen(const QString &path);
    void refreshGpsPorts();
    void fillFromLocator();
    void chooseSymbol();
    void updateSymbolDisplay();
    void updateSmartPreview();
    void updateRetryPreview();
    void browseSound();

    AppConfig m_config;

    // station
    QLineEdit *m_callsignEdit = nullptr;
    QSpinBox *m_ssidSpin = nullptr;
    QLineEdit *m_destinationEdit = nullptr;
    QLineEdit *m_digiEdit = nullptr;
    QLineEdit *m_nameEdit = nullptr;
    QLineEdit *m_locatorEdit = nullptr;
    QComboBox *m_encodingCombo = nullptr;
    QSpinBox *m_maxInfoSpin = nullptr;
    // modem
    QLineEdit *m_modemConfigEdit = nullptr;
    QSpinBox *m_modemChannelSpin = nullptr;
    QCheckBox *m_modemAutoStartCheck = nullptr;
    QCheckBox *m_modemEchoCheck = nullptr;
    QCheckBox *m_sendParamsCheck = nullptr;
    QSpinBox *m_txdelaySpin = nullptr;
    QSpinBox *m_persistSpin = nullptr;
    QSpinBox *m_slottimeSpin = nullptr;
    QSpinBox *m_txtailSpin = nullptr;
    QCheckBox *m_fullDuplexCheck = nullptr;
    // channel access
    QCheckBox *m_useDcdCheck = nullptr;
    QSpinBox *m_holdOffSpin = nullptr;
    QSpinBox *m_gapSpin = nullptr;
    QSpinBox *m_jitterSpin = nullptr;
    QSpinBox *m_maxDeferSpin = nullptr;
    QCheckBox *m_waitTxbufCheck = nullptr;
    QCheckBox *m_ownEchoCheck = nullptr;
    // beacon
    QCheckBox *m_beaconCheck = nullptr;
    QSpinBox *m_beaconIntervalSpin = nullptr;
    QLineEdit *m_beaconTextEdit = nullptr;
    QCheckBox *m_beaconOnConnectCheck = nullptr;
    QLineEdit *m_beaconDestEdit = nullptr;
    QLineEdit *m_beaconDigiEdit = nullptr;
    // position
    QCheckBox *m_posEnabledCheck = nullptr;
    QSpinBox *m_posIntervalSpin = nullptr;
    QComboBox *m_posSourceCombo = nullptr;
    QDoubleSpinBox *m_latSpin = nullptr;
    QDoubleSpinBox *m_lonSpin = nullptr;
    QDoubleSpinBox *m_altSpin = nullptr;
    QComboBox *m_gpsPortCombo = nullptr;
    QComboBox *m_gpsBaudCombo = nullptr;
    QSpinBox *m_gpsMaxAgeSpin = nullptr;
    QCheckBox *m_gpsCourseCheck = nullptr;
    QCheckBox *m_gpsAutoOpenCheck = nullptr;
    QCheckBox *m_sbCheck = nullptr;
    QDoubleSpinBox *m_sbLowSpin = nullptr;
    QDoubleSpinBox *m_sbHighSpin = nullptr;
    QSpinBox *m_sbSlowSpin = nullptr;
    QSpinBox *m_sbFastSpin = nullptr;
    QDoubleSpinBox *m_sbTurnMinSpin = nullptr;
    QDoubleSpinBox *m_sbTurnSlopeSpin = nullptr;
    QSpinBox *m_sbTurnTimeSpin = nullptr;
    QLabel *m_sbPreviewLabel = nullptr;
    QLineEdit *m_symbolTableEdit = nullptr;
    QLineEdit *m_symbolCodeEdit = nullptr;
    QLabel *m_symbolLabel = nullptr;
    QLineEdit *m_posCommentEdit = nullptr;
    QCheckBox *m_sendAltitudeCheck = nullptr;
    QCheckBox *m_messagingCapableCheck = nullptr;
    QLineEdit *m_posDestEdit = nullptr;
    QLineEdit *m_posDigiEdit = nullptr;
    QCheckBox *m_posOnConnectCheck = nullptr;
    // messaging
    QCheckBox *m_autoAckCheck = nullptr;
    QSpinBox *m_maxAttemptsSpin = nullptr;
    QSpinBox *m_retrySecondsSpin = nullptr;
    QLabel *m_retryPreviewLabel = nullptr;
    QCheckBox *m_trackStationsCheck = nullptr;
    QSpinBox *m_stationExpirySpin = nullptr;
    // notifications
    QCheckBox *m_notifyEnabledCheck = nullptr;

    QWidget *buildAprsIsGroup();
    QCheckBox *m_modemEnabledCheck = nullptr;
    QComboBox *m_mapProviderCombo = nullptr;
    QLineEdit *m_mapUrlEdit = nullptr;
    QCheckBox *m_mapSharpCheck = nullptr;
    QSpinBox *m_mapCacheSpin = nullptr;
    QLabel *m_mapCacheLabel = nullptr;
    QCheckBox *m_isEnabledCheck = nullptr;
    QComboBox *m_isRegionCombo = nullptr;
    QLineEdit *m_isHostEdit = nullptr;
    QSpinBox *m_isPortSpin = nullptr;
    QLineEdit *m_isPasscodeEdit = nullptr;
    QCheckBox *m_isGateCheck = nullptr;
    QCheckBox *m_isReceiveCheck = nullptr;
    QSpinBox *m_isRadiusSpin = nullptr;
    QCheckBox *m_isShowCheck = nullptr;
    QCheckBox *m_isPositionCheck = nullptr;
    QCheckBox *m_isPositionOnConnectCheck = nullptr;
    QCheckBox *m_notifySoundCheck = nullptr;
    QCheckBox *m_notifyFlashCheck = nullptr;
    QLineEdit *m_soundFileEdit = nullptr;
    QSpinBox *m_volumeSpin = nullptr;
    QSpinBox *m_minIntervalSpin = nullptr;
    QCheckBox *m_onMessageCheck = nullptr;
    QCheckBox *m_onChatCheck = nullptr;
    QCheckBox *m_onBulletinCheck = nullptr;
    QCheckBox *m_onWeatherCheck = nullptr;
    QCheckBox *m_onPositionCheck = nullptr;
    QCheckBox *m_onStatusCheck = nullptr;
    // interface
    QComboBox *m_styleCombo = nullptr;
    QCheckBox *m_timestampsCheck = nullptr;
    QCheckBox *m_monitorCheck = nullptr;
    QSpinBox *m_fontSizeSpin = nullptr;
    QSpinBox *m_maxLinesSpin = nullptr;
    QLabel *m_pathLabel = nullptr;
};
