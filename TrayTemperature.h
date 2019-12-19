/****************************************************************************

TrayTemperature.h - A Qt program to display the outdoor temperature
                    in the system icon tray

Copyright 2019 Doug Bloebaum

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
****************************************************************************/

#ifndef TRAYTEMPERATURE_H
#define TRAYTEMPERATURE_H

#ifndef QT_NO_SYSTEMTRAYICON

#include "ui_TrayTemperatureConfig.h"
#include "ui_TrayTemperatureAbout.h"
#include "TimedMessageBox.h"

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

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void timerTick();
    void handleWeatherNetworkData(QNetworkReply *networkReply);
    void testWeatherNetworkData(QNetworkReply *rep);
    void handleGeoLocationData(QNetworkReply *rep);
    void refreshLocation();
    void refreshTemperature();
    void displayIcon();
    void fireAndRestartTimer();

    void on_button_font_clicked();
    void on_button_fgColor_clicked();
    void on_button_bgColor_clicked();
    void on_checkBox_transparentBg_stateChanged(int arg1);
    void on_radio_useIpLocation_clicked();
    void on_radio_useManualLocation_clicked();
    void on_buttonBox_main_clicked(QAbstractButton *button);
    void on_lineEdit_openWeatherApiKey_textChanged(const QString &arg1);
    void on_checkBox_showDegreeSymbol_stateChanged(int arg1);
    void on_checkBox_autoSizeFont_stateChanged(int arg1);
    void on_pushButton_testApiKey_clicked();

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
        bool autoAdjustFontSize;
        bool showDegreeSymbol;

        const QString temperatureDisplayUnitsDefault = "IMPERIAL";
        const int temperatureUpdateFrequencyDefault = 10;
        const QFont temperatureFontDefault = QFont("Segoe UI", 9, QFont::Bold, false);
        const QColor temperatureColorDefault = QColor(Qt::white);
        const QColor bgTemperatureColorDefault = QColor(Qt::blue);
        const bool bgTemperatureColorTransparentDefault = true;
        const bool useManualLocationDefault = false;
        const double manualLatDefault = 0;
        const double manualLonDefault = 0;
        const bool autoAdjustFontSizeDefault = true;
        const bool showDegreeSymbolDefault = true;
    };

    const QIcon warningIcon = QIcon(":/images/c-warning.svg");

    const QString version = QString("%1").arg(GIT_VERSION);

    void updateConfigDialogWidgets();
    void updateTemperatureDisplaySampleLabel();
    void createActions();
    void createTrayIcon();
    void createConfigDialogConnections();
    void loadSettings();
    void saveSettings();
    void defaultSettings();
    void popupNetworkWarning(QNetworkReply *rep, const QString& msg);
    void showAboutDialog();
    void showConfigDialog();
    void highlightIfEmpty(QLineEdit *e);
    QFont adjustFontSizeForTrayIcon(QFont font, QString s);

    // System tray icon size is 16x16 on Windows, and 22x22 on X11.
    // would be nice if we could query the icon to find its size,
    // but trayIcon->geometry() doesn't seem to do that
    const int systemTrayIconSize = QSysInfo::productType() == "windows" ? 16 : 22;

    Ui::TrayTemperatureConfig *ui;
    Ui::TrayTemperatureAbout *aboutUi;

    QDialog *aboutDialog = nullptr;

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
    QAction *aboutAction;
    QAction *quitAction;

    QSystemTrayIcon *trayIcon;
    QMenu *trayIconMenu;

    QFont holdMyFont;

    const int minRetryWaitSeconds = 10;
    int retryWaitSeconds = minRetryWaitSeconds;

signals:
    void locationRefreshed();
    void temperatureRefreshed();
};
//! [0]

#endif // QT_NO_SYSTEMTRAYICON

#endif
