/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef TRAYTEMPERATURE_H
#define TRAYTEMPERATURE_H

#ifndef QT_NO_SYSTEMTRAYICON

#include "ui_TrayTemperatureConfig.h"
#include <QDialog>

QT_BEGIN_NAMESPACE
class QAction;
class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QMenu;
class QPushButton;
class QSpinBox;
class QTextEdit;
class QRadioButton;
class QFrame;
class QDialogButtonBox;
class QSystemTrayIcon;
class QSettings;
class QTimer;
class QNetworkAccessManager;
class QNetworkConfigurationManager;
class QNetworkSession;
class QNetworkReply;
class QCheckBox;
QT_END_NAMESPACE

//! [0]
class TrayTemperature : public QDialog
{
    Q_OBJECT

public:
    TrayTemperature();

    void setVisible(bool visible) override;

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void timerTick();

    void handleWeatherNetworkData(QNetworkReply *networkReply);
    void handleGeoLocationData(QNetworkReply *rep);
    void refreshLocation();
    void refreshTemperature();
    void displayIcon();
    void fireAndRestartTimer();

    void on_buttonBox_OK_Cancel_accepted();

    void on_buttonBox_OK_Cancel_rejected();

    void on_button_font_clicked();

    void on_button_fgColor_clicked();

    void on_button_bgColor_clicked();

    void on_checkBox_transparentBg_stateChanged(int arg1);

    void on_button_restore_clicked();

    void on_radio_useIpLocation_clicked();

    void on_radio_useManualLocation_clicked();

private:

    class SettingsHolder {
    public:
        QString temperatureDisplayUnits;
        int temperatureUpdateFrequency;
        QFont temperatureFont;
        QColor temperatureColor;
        QColor bgTemperatureColor;
        QString APIKey;
        bool bgTemperatureColorTransparent;
        bool useManualLocation;
        double manualLat;
        double manualLon;

        const QString temperatureDisplayUnitsDefault = "imperial";
        const int temperatureUpdateFrequencyDefault = 5;
        const QFont temperatureFontDefault = QFont("Segoe UI", 9, QFont::Bold, false);
        const QColor temperatureColorDefault = QColor(Qt::white);
        const QColor bgTemperatureColorDefault = QColor(Qt::blue);
        const bool bgTemperatureColorTransparentDefault = true;
        const bool useManualLocationDefault = false;
        double manualLatDefault = 0;
        double manualLonDefault = 0;
    };

    const QIcon warningIcon = QIcon(":/images/c-warning.svg");
    void updateConfigDialogWidgets();
    void updateTemperatureDisplaySampleLabel();
    void createActions();
    void createTrayIcon();
    void createConfigDialogConnections();
    void loadSettings();
    void saveSettings();
    void defaultSettings();
    void popupNetworkWarning(QNetworkReply *rep, QString msg);

    // System tray icon size is 16x16 on Windows, and 22x22 on X11.
    // would be nice if we could query the icon to find its size,
    // but trayIcon->geometry() doesn't seem to do that
    const int systemTrayIconSize = QSysInfo::productType() == "windows" ? 16 : 22;

    Ui::TrayTemperatureConfig *ui;

    QSettings *settings;
    SettingsHolder *settingsHolder;
    QNetworkAccessManager *nam;
    QNetworkConfigurationManager *ncm;
    QNetworkSession *ns;
    QTimer *timer;

    double temperature;
    double lat;
    double lon;
    QString city;

    QAction *minimizeAction;
    QAction *maximizeAction;
    QAction *configureAction;
    QAction *refreshAction;
    QAction *quitAction;

    QSystemTrayIcon *trayIcon;
    QMenu *trayIconMenu;

signals:
    void locationRefreshed();
    void temperatureRefreshed();
};
//! [0]

#endif // QT_NO_SYSTEMTRAYICON

#endif
