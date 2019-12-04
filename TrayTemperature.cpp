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

#include "ui_TrayTemperatureConfig.h"
#include "TrayTemperature.h"

#ifndef QT_NO_SYSTEMTRAYICON

#include <QAction>
#include <QCoreApplication>
#include <QCloseEvent>
#include <QMenu>
#include <QPainter>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>
#include <QSettings>
#include <QNetworkConfigurationManager>
#include <QNetworkAccessManager>
#include <QNetworkSession>
#include <QNetworkReply>
#include <QTimer>
#include <QSystemTrayIcon>
#include <QFontDialog>
#include <QColorDialog>
#include <QWidgetAction>
#include <QMessageBox>

TrayTemperature::TrayTemperature()
{
    qDebug() << "constructing TrayTemperature";

    ui = new Ui::TrayTemperatureConfig;
    ui->setupUi(this);

    this->settings = new QSettings("Doug Bloebaum", "TrayTemperature");
    this->settingsHolder = new SettingsHolder;
    loadSettings();

    //createConfigDialog();
    createActions();
    createTrayIcon();

    this->nam = new QNetworkAccessManager();
    this->ncm = new QNetworkConfigurationManager();
    this->ns = new QNetworkSession(this->ncm->defaultConfiguration());

    // tell the system we want network
    ns->open();

    connect(this, &TrayTemperature::locationRefreshed, this, &TrayTemperature::refreshTemperature);
    connect(this, &TrayTemperature::temperatureRefreshed, this, &TrayTemperature::displayIcon);
    connect(this, &TrayTemperature::locationRefreshed, this, &TrayTemperature::updateConfigDialogWidgets);

    this->trayIcon->setIcon(QIcon(":/images/c-weather-cloudy-with-sun.svg"));
    trayIcon->show();

    this->timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &TrayTemperature::timerTick);
    timer->start(60 * 1000 * settingsHolder->temperatureUpdateFrequency);
    timerTick(); // do the first timer loop on startup

    setWindowTitle(tr("Tray Temperature"));

    if (QString(settingsHolder->APIKey).isEmpty()) this->show();
}

void TrayTemperature::setVisible(bool visible)
{
    qDebug() << QString("setVisible(%1)").arg(visible);
    minimizeAction->setEnabled(visible);
    configureAction->setEnabled(isMaximized() || !visible);
    //updateConfigDialogWidgets();
    QDialog::setVisible(visible);
}

void TrayTemperature::closeEvent(QCloseEvent *event)
{
#ifdef Q_OS_OSX
    if (!event->spontaneous() || !isVisible()) {
        return;
    }
#endif
    if (trayIcon->isVisible()) {
        // we hide and keep running instead of quitting the application
        hide();
        event->ignore();
    }
}

void TrayTemperature::timerTick() {
    qDebug() << "timerTick()";
    // set the tray icon to an hourglass during the refresh process
    this->trayIcon->setIcon(QIcon(":/images/c-hourglass.svg"));
    this->refreshLocation();
}

void TrayTemperature::refreshLocation() {
    qDebug() << "refreshLocation()";

    if (settingsHolder->useManualLocation) {
        qDebug() << "using manually entered lat/lon";
        emit(locationRefreshed());
    }
    else {
        QUrl url("http://ip-api.com/json?fields=57552");
        qDebug() << "submitting ip-api.com request";

        QNetworkReply *rep = nam->get(QNetworkRequest(url));
        // connect up the signal right away
        connect(rep, &QNetworkReply::finished,
                this, [this, rep]() { handleGeoLocationData(rep); });
    }
}

void TrayTemperature::popupNetworkWarning(QNetworkReply *rep, QString msg) {
    trayIcon->setIcon(warningIcon);
    trayIcon->setToolTip(msg + " request error");

    QMessageBox msgBox;
    msgBox.setWindowTitle("Tray Temperature");
    msgBox.setText(QString("Problem requesting " + msg + " data. "
                           "Right-click the tray icon, then choose 'Refresh Temperature...' to retry. "
                           "Right-click the tray icon, then choose 'Configure...' to fix configuration."));
    QString ts = QDateTime::currentDateTime().toString();
    msgBox.setDetailedText(QString("Time: %1\nError: %2\nRequest: %3").arg(ts).arg(rep->errorString()).arg(rep->url().toString()));
    msgBox.exec();

}

void TrayTemperature::handleGeoLocationData(QNetworkReply *rep) {
    qDebug() << "handleGeoLocationData()";
    if (!rep) {
        trayIcon->setIcon(warningIcon);
        trayIcon->setToolTip("Geolocation request error");
        qDebug() << "oops, error with geolocation reply";
        trayIcon->showMessage("Geolocation request error", "No reply received");
        return;
    }

    if (!rep->error()) {
        QJsonDocument geoJsonDoc = QJsonDocument::fromJson(rep->readAll());
        qDebug() << "geo location data: " << geoJsonDoc;

        QVariantMap geoVMap = geoJsonDoc.object().toVariantMap();
        this->lat = geoVMap["lat"].toDouble();
        this->lon = geoVMap["lon"].toDouble();
        this->city = geoVMap["city"].toString();

        emit(locationRefreshed());
    }
    else {
        popupNetworkWarning(rep, "geolocation");
        timer->stop();
    }

}

void TrayTemperature::refreshTemperature() {
    qDebug() << "refreshTemperature()";
    QUrl url("http://api.openweathermap.org/data/2.5/weather");
    QUrlQuery query;

    double lat = settingsHolder->useManualLocation ? settingsHolder->manualLat : this->lat;
    double lon = settingsHolder->useManualLocation ? settingsHolder->manualLon : this->lon;
    query.addQueryItem("lat", QString::number(lat));
    query.addQueryItem("lon", QString::number(lon));
    query.addQueryItem("mode", "json");
    query.addQueryItem("units", settingsHolder->temperatureDisplayUnits);
    query.addQueryItem("appid", settingsHolder->APIKey);
    url.setQuery(query);
    qDebug() << "submitting request: " << url;

    QNetworkReply *rep = nam->get(QNetworkRequest(url));
    // connect up the signal right away
    connect(rep, &QNetworkReply::finished,
            this, [this, rep]() { handleWeatherNetworkData(rep); });
}

void TrayTemperature::handleWeatherNetworkData(QNetworkReply *rep){
    qDebug() << "handleWeatherNetworkData()";
    if (!rep) {
        trayIcon->setIcon(warningIcon);
        trayIcon->setToolTip("Weather request error");
        trayIcon->showMessage("Weather network error", "No reply received");
        qDebug() << "oops, error with weather reply";
        return;
    }

    if (!rep->error()) {
        QJsonDocument weatherJsonDoc = QJsonDocument::fromJson(rep->readAll());
        QVariantMap weatherVMap = weatherJsonDoc.object().toVariantMap();
        QVariantMap mainVMap = weatherVMap["main"].toMap();
        this->temperature = mainVMap["temp"].toDouble();
        qDebug() << "temperature is: " << this->temperature;
        emit(temperatureRefreshed());
    }
    else {
        popupNetworkWarning(rep, "temperature");
        timer->stop();
    }
}


void TrayTemperature::displayIcon() {

    QPixmap pix(systemTrayIconSize,systemTrayIconSize);
    pix.fill(settingsHolder->bgTemperatureColor);

    const QRect rectangle = QRect(0, 0, systemTrayIconSize, systemTrayIconSize);

    QPainter painter( &pix );
    painter.setPen(settingsHolder->temperatureColor);
    painter.setFont(settingsHolder->temperatureFont);
    QString temperatureString = QString::number(qRound(this->temperature));
    qDebug() << "temperatureString=" << temperatureString;
    painter.drawText(rectangle, Qt::AlignCenter|Qt::AlignVCenter, temperatureString);
    painter.end();
    QIcon myIcon(pix);

    QString units = settingsHolder->temperatureDisplayUnits == "metric" ? "°C" : "°F";
    QString toolTipString = "Temp";
    toolTipString += this->city.isEmpty() ? "" : " in " + this->city;
    toolTipString += QString(" is %1%2").arg(this->temperature).arg(units);

    trayIcon->setIcon(myIcon);
    trayIcon->setToolTip(toolTipString);
    trayIcon->show();

}

//! [3]

void TrayTemperature::updateTemperatureDisplaySampleLabel() {
    ui->label_sampleDisplay->setFont(settingsHolder->temperatureFont);
    ui->label_sampleDisplay->setText("42");
    ui->label_sampleDisplay->setAutoFillBackground(true);
    ui->label_sampleDisplay->setFixedSize(systemTrayIconSize,systemTrayIconSize);

    QPalette palette = ui->label_sampleDisplay->palette();
    palette.setColor(ui->label_sampleDisplay->backgroundRole(), settingsHolder->bgTemperatureColor);
    palette.setColor(ui->label_sampleDisplay->foregroundRole(), settingsHolder->temperatureColor);
    ui->label_sampleDisplay->setPalette(palette);
}

QString fontDisplayName(QFont font) {
    return font.family() + " " +
            QString::number(font.pointSize()) + " " +
            (font.bold() ? "bold " : "") +
            (font.italic() ? "italic " : "") +
            (font.underline() ? "underline " : "");
}

void TrayTemperature::updateConfigDialogWidgets() {
    qDebug() << "updateConfigDialogWidgets()";
    ui->radio_imperial->setChecked(settingsHolder->temperatureDisplayUnits=="imperial");
    ui->radio_metric->setChecked(settingsHolder->temperatureDisplayUnits=="metric");

    ui->label_font->setText(fontDisplayName(settingsHolder->temperatureFont));
    ui->label_fgColor->setText(settingsHolder->temperatureColor.name());
    ui->label_bgColor->setText(settingsHolder->bgTemperatureColor.name());

    ui->lineEdit_openWeatherApiKey->setText(settingsHolder->APIKey);

    ui->spinBox_updateFrequency->setValue(settingsHolder->temperatureUpdateFrequency);
    ui->checkBox_transparentBg->setChecked(settingsHolder->bgTemperatureColorTransparent);
    ui->button_bgColor->setDisabled(settingsHolder->bgTemperatureColorTransparent);
    ui->label_bgColor->setDisabled(settingsHolder->bgTemperatureColorTransparent);

    double lat = settingsHolder->useManualLocation ? settingsHolder->manualLat : this->lat;
    double lon = settingsHolder->useManualLocation ? settingsHolder->manualLon : this->lon;

    ui->radio_useIpLocation->setChecked(!settingsHolder->useManualLocation);
    ui->radio_useManualLocation->setChecked(settingsHolder->useManualLocation);

    qDebug() << "lat=" << lat << "lon=" << lon;
    ui->spinBox_lat->setValue(lat);
    ui->spinBox_lon->setValue(lon);

    ui->spinBox_lat->setEnabled(settingsHolder->useManualLocation);
    ui->spinBox_lon->setEnabled(settingsHolder->useManualLocation);

    if (settingsHolder->useManualLocation) {
        this->city = "";
    }

    updateTemperatureDisplaySampleLabel();
}

void TrayTemperature::loadSettings() {
    settingsHolder->temperatureUpdateFrequency = settings->value("temperatureUpdateFrequency").toInt();
    settingsHolder->APIKey = settings->value("APIKey").toString();
    settingsHolder->temperatureDisplayUnits = settings->value("temperatureDisplayUnits").toString();

    settingsHolder->temperatureFont = settings->value("temperatureFont").value<QFont>();
    settingsHolder->temperatureColor = settings->value("temperatureColor").value<QColor>();
    settingsHolder->bgTemperatureColor = settings->value("bgTemperatureColor").value<QColor>();
    settingsHolder->bgTemperatureColorTransparent = settings->value("bgTemperatureColorTransparent").toBool();
    settingsHolder->bgTemperatureColor.setAlpha(settingsHolder->bgTemperatureColorTransparent ? 0 : 255);

    settingsHolder->useManualLocation = settings->value("useManualLocation").toBool();
    settingsHolder->manualLat = settings->value("manualLat").toDouble();
    settingsHolder->manualLon = settings->value("manualLon").toDouble();
}

void TrayTemperature::saveSettings() {
    qDebug() << "saveSettings()";

    settings->setValue("temperatureUpdateFrequency", settingsHolder->temperatureUpdateFrequency);
    settings->setValue("APIKey", settingsHolder->APIKey);
    settings->setValue("temperatureDisplayUnits", settingsHolder->temperatureDisplayUnits);

    settings->setValue("temperatureFont", settingsHolder->temperatureFont);
    settings->setValue("temperatureColor", settingsHolder->temperatureColor);
    settings->setValue("bgTemperatureColor", settingsHolder->bgTemperatureColor);
    settings->setValue("bgTemperatureColorTransparent", settingsHolder->bgTemperatureColorTransparent);

    settings->setValue("useManualLocation", settingsHolder->useManualLocation);
    settings->setValue("manualLat", settingsHolder->manualLat);
    settings->setValue("manualLon", settingsHolder->manualLon);
}

void TrayTemperature::defaultSettings() {
    qDebug() << "defaultSettings()";

    settingsHolder->temperatureUpdateFrequency = settingsHolder->temperatureUpdateFrequencyDefault;
    settingsHolder->temperatureDisplayUnits = settingsHolder->temperatureDisplayUnitsDefault;
    settingsHolder->temperatureFont = settingsHolder->temperatureFontDefault;
    settingsHolder->temperatureColor = settingsHolder->temperatureColorDefault;
    settingsHolder->bgTemperatureColor = settingsHolder->bgTemperatureColorDefault;
    settingsHolder->bgTemperatureColorTransparent = settingsHolder->bgTemperatureColorTransparentDefault;
    settingsHolder->bgTemperatureColor.setAlpha(settingsHolder->bgTemperatureColorTransparent ? 0 : 255);
    settingsHolder->useManualLocation = settingsHolder->useManualLocationDefault;
    settingsHolder->manualLat = settingsHolder->manualLatDefault;
    settingsHolder->manualLon = settingsHolder->manualLonDefault;
}

void TrayTemperature::fireAndRestartTimer() {
    timerTick();
    this->timer->start();
}

void TrayTemperature::createActions()
{
    qDebug() << "createActions()";
    minimizeAction = new QAction(tr("Mi&nimize"), this);
    connect(minimizeAction, &QAction::triggered, this, &QWidget::hide);

    configureAction = new QAction(tr("&Configure..."), this);
    connect(configureAction, &QAction::triggered, this, &QWidget::showNormal);

    refreshAction = new QAction(tr("Refresh &Temperature..."), this);
    connect(refreshAction, &QAction::triggered, this, &TrayTemperature::fireAndRestartTimer);

    quitAction = new QAction(tr("&Quit"), this);
    connect(quitAction, &QAction::triggered, qApp, &QCoreApplication::quit);
}

void TrayTemperature::createTrayIcon()
{
    qDebug() << "createTrayIcon()";

    trayIconMenu = new QMenu(this);

    QLabel* titleLabel = new QLabel(tr("<b>TrayTemperature</b>"), this);
    QWidgetAction* titleAction = new QWidgetAction(trayIconMenu);
    titleAction->setDefaultWidget(titleLabel);

    trayIconMenu->addAction(titleAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(configureAction);
    trayIconMenu->addAction(refreshAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(quitAction);

    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setContextMenu(trayIconMenu);
}

#endif


void TrayTemperature::on_buttonBox_OK_Cancel_accepted()
{
    qDebug() << "on_buttonBox_OK_Cancel_accepted()";
    settingsHolder->temperatureUpdateFrequency = ui->spinBox_updateFrequency->value();
    settingsHolder->APIKey = ui->lineEdit_openWeatherApiKey->text();
    settingsHolder->temperatureDisplayUnits = ui->radio_metric->isChecked() ? "metric" : "imperial";
    qDebug() << "ui->checkBox_transparentBg->isChecked(): " << ui->checkBox_transparentBg->isChecked();
    settingsHolder->bgTemperatureColorTransparent = ui->checkBox_transparentBg->isChecked();
    settingsHolder->temperatureFont = ui->label_sampleDisplay->font();
    settingsHolder->temperatureColor = ui->label_sampleDisplay->palette().color(ui->label_sampleDisplay->foregroundRole());
    settingsHolder->bgTemperatureColor = ui->label_sampleDisplay->palette().color(ui->label_sampleDisplay->backgroundRole());
    settingsHolder->useManualLocation = ui->radio_useManualLocation->isChecked();
    settingsHolder->manualLat = ui->spinBox_lat->value();
    settingsHolder->manualLon = ui->spinBox_lon->value();

    saveSettings();

    timer->setInterval(60 * 1000 * settingsHolder->temperatureUpdateFrequency);
    fireAndRestartTimer();
}

void TrayTemperature::on_buttonBox_OK_Cancel_rejected()
{   //restore widgets' states from settingsHolder
    updateConfigDialogWidgets();
}

void TrayTemperature::on_button_font_clicked()
{
    bool ok;
    QFont font = QFontDialog::getFont(&ok, settingsHolder->temperatureFont);
    if (ok) {
        qDebug() << "font is now: " << font;
        //settingsHolder->temperatureFont = font;
        ui->label_font->setText(fontDisplayName(font));
        ui->label_sampleDisplay->setFont(font);

        //updateTemperatureDisplaySampleLabel();
    } else {
        qDebug() << "font is still: " << font;
    }
}

void TrayTemperature::on_button_fgColor_clicked()
{
    const QColor color = QColorDialog::getColor(settingsHolder->temperatureColor);
    if (color.isValid()) {
        qDebug() << "color is now" << color;
        //settingsHolder->temperatureColor = color;
        ui->label_fgColor->setText(color.name());

        QPalette palette = ui->label_sampleDisplay->palette();
        palette.setColor(ui->label_sampleDisplay->foregroundRole(), color);
        ui->label_sampleDisplay->setPalette(palette);

        //updateTemperatureDisplaySampleLabel();
    }
    else {
        qDebug() << "color is still" << color;
    }
}

void TrayTemperature::on_button_bgColor_clicked()
{
    QColor color = QColorDialog::getColor(
        settingsHolder->bgTemperatureColor,
        nullptr,
        nullptr,
        QColorDialog::ShowAlphaChannel
    );
    if (color.isValid()) {
        qDebug() << "bg color is now" << color;
        ui->label_bgColor->setText(color.name());

        QPalette palette = ui->label_sampleDisplay->palette();
        color.setAlpha(ui->checkBox_transparentBg ? 0 : 255);
        palette.setColor(ui->label_sampleDisplay->backgroundRole(), color);
        ui->label_sampleDisplay->setPalette(palette);
    }
    else {
        qDebug() << "color is still" << color;
    }
}

void TrayTemperature::on_checkBox_transparentBg_stateChanged(int state)
{
    int alpha;
    qDebug() << QString("on_checkBox_transparentBg_stateChanged(%1)").arg(state);
    if (state == Qt::Checked) {
        ui->button_bgColor->setDisabled(true);
        ui->label_bgColor->setDisabled(true);
        alpha = 0;
    }
    else {
        ui->button_bgColor->setDisabled(false);
        ui->label_bgColor->setDisabled(false);
        alpha = 255;
    }

    QPalette palette = ui->label_sampleDisplay->palette();
    QColor color = palette.color(ui->label_sampleDisplay->backgroundRole());
    color.setAlpha(alpha);
    palette.setColor(ui->label_sampleDisplay->backgroundRole(), color);
    ui->label_sampleDisplay->setPalette(palette);
}

void TrayTemperature::on_button_restore_clicked()
{
    qDebug() << "on_button_restore_clicked()";
    defaultSettings();
    updateConfigDialogWidgets();
    fireAndRestartTimer();
}

void TrayTemperature::on_radio_useIpLocation_clicked()
{
    qDebug() << "on_radio_useIpLocation_clicked()";
    settingsHolder->useManualLocation = false;
    ui->spinBox_lat->setDisabled(true);
    ui->spinBox_lon->setDisabled(true);
}

void TrayTemperature::on_radio_useManualLocation_clicked()
{
    qDebug() << "on_radio_useManualLocation_clicked()";
    settingsHolder->useManualLocation = true;
    ui->spinBox_lat->setDisabled(false);
    ui->spinBox_lon->setDisabled(false);
}
