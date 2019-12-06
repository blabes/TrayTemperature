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
#include "ui_TrayTemperatureAbout.h"
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
    qDebug() << "constructing TrayTemperature version " << version;

    ui = new Ui::TrayTemperatureConfig;
    ui->setupUi(this);

    this->settings = new QSettings("Doug Bloebaum", "TrayTemperature");
    this->settingsHolder = new SettingsHolder;
    if (this->settings->contains("APIKey")) {
        // this ain't our first rodeo: load from saved settings
        loadSettings();
    }
    else {
        // hey, this IS our first rodeo: load some reasonable defaults
        defaultSettings();
    }

    createActions();
    createTrayIcon();

    this->nam = new QNetworkAccessManager();
    this->ncm = new QNetworkConfigurationManager();
    this->ns = new QNetworkSession(this->ncm->defaultConfiguration());

    // tell the system we want network
    ns->open();

    setWindowTitle(tr("Tray Temperature"));

    connect(this, &TrayTemperature::locationRefreshed, this, &TrayTemperature::refreshTemperature);
    connect(this, &TrayTemperature::temperatureRefreshed, this, &TrayTemperature::displayIcon);
    connect(this, &TrayTemperature::locationRefreshed, this, &TrayTemperature::updateConfigDialogWidgets);

    this->trayIcon->setIcon(QIcon(":/images/c-weather-cloudy-with-sun.svg"));
    trayIcon->show();

    this->timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &TrayTemperature::timerTick);
    int frequencySeconds = 60*1000*settingsHolder->temperatureUpdateFrequency;
    if (frequencySeconds < 60) qFatal("timer frequency too low"); // sanity check that timer waits at least 1 minute

    if (QString(settingsHolder->APIKey).isEmpty()){
        // no API Key... maybe the first run?  Show the config form.
        updateConfigDialogWidgets();
        this->showConfigDialog();
    }
    else {
        timer->start(frequencySeconds);
        timerTick(); // do the first timer loop on normal startup
    }
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
        connect(rep, &QNetworkReply::finished, this, [this, rep]() { handleGeoLocationData(rep); });
    }
}

void TrayTemperature::popupNetworkWarning(QNetworkReply *rep, QString msg) {
    trayIcon->setIcon(warningIcon);
    trayIcon->setToolTip(msg + " request error");

    QMessageBox msgBox;
    msgBox.setWindowTitle("Tray Temperature");
    msgBox.setText(QString("Problem requesting " + msg + " data, "
                           "so automatic temperature updates have been suspended. "
                           "Click 'Retry' to retry the request and restart automatic updates. "
                           "Click 'Configure...' if you need to fix the configuration."));
    QString ts = QDateTime::currentDateTime().toString();
    msgBox.setDetailedText(QString("Time: %1\nError: %2\nRequest: %3").arg(ts, rep->errorString(), rep->url().toString()));

    // add "Configure..." and "Retry" buttons for convenience, and the standard "Cancel"
    QAbstractButton *retryButton = msgBox.addButton("Retry", QMessageBox::AcceptRole);
    QAbstractButton *configureButton = msgBox.addButton("Configure...", QMessageBox::ActionRole);
    msgBox.setStandardButtons(QMessageBox::Cancel);

    msgBox.exec();

    if (msgBox.clickedButton() == configureButton) {
        showConfigDialog();
    }
    else if (msgBox.clickedButton() == retryButton){
        fireAndRestartTimer();
    }

}

void TrayTemperature::handleGeoLocationData(QNetworkReply *rep) {
    qDebug() << "handleGeoLocationData()";
    if (!rep) {
        trayIcon->setIcon(warningIcon);
        trayIcon->setToolTip("Geolocation request error");
        trayIcon->showMessage("Geolocation request error", "Reply was null");
        qFatal("error with geolocation reply, rep was null");
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
        timer->stop();
        popupNetworkWarning(rep, "geolocation");
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
        trayIcon->showMessage("Weather network error", "Reply was null");
        qFatal("error with weather reply, rep was null");
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
        timer->stop();
        popupNetworkWarning(rep, "temperature");
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

    QString units = settingsHolder->temperatureDisplayUnits == "METRIC" ? "°C" : "°F";
    QString toolTipString = "Temp";
    toolTipString += this->city.isEmpty() ? "" : " in " + this->city;
    toolTipString += QString(" is %1%2").arg(this->temperature).arg(units);

    trayIcon->setIcon(myIcon);
    trayIcon->setToolTip(toolTipString);
    trayIcon->show();

}

//! [3]

void TrayTemperature::updateTemperatureDisplaySampleLabel() { // TODO: use stylesheet?
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
    ui->radio_imperial->setChecked(settingsHolder->temperatureDisplayUnits == "IMPERIAL");
    ui->radio_metric->setChecked(settingsHolder->temperatureDisplayUnits == "METRIC");

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

void TrayTemperature::showAboutDialog() {

    if (aboutDialog == nullptr) {
        aboutDialog = new QDialog(nullptr, Qt::WindowTitleHint | Qt::WindowCloseButtonHint);
        aboutUi = new Ui::TrayTemperatureAbout;
        aboutUi->setupUi(aboutDialog);
        aboutUi->label_version->setText(version);
    }
    aboutDialog->showNormal();
    aboutDialog->raise();
}

void TrayTemperature::showConfigDialog() {

    this->showNormal();
    this->raise();

    // if the APIKey is empty, set its background to yellowish as an alert
    if (ui->lineEdit_openWeatherApiKey->text().isEmpty()) {
        ui->lineEdit_openWeatherApiKey->setStyleSheet("background-color: rgb(255,255,127)");
    }
    else { // white
        ui->lineEdit_openWeatherApiKey->setStyleSheet("background-color: rgb(255,255,255)");
    }

}

void TrayTemperature::createActions()
{
    qDebug() << "createActions()";

    configureAction = new QAction(tr("&Configure..."), this);
    connect(configureAction, &QAction::triggered, this, &TrayTemperature::showConfigDialog);

    refreshAction = new QAction(tr("Refresh &Temperature..."), this);
    connect(refreshAction, &QAction::triggered, this, &TrayTemperature::fireAndRestartTimer);

    aboutAction = new QAction(tr("&Help/About..."), this);
    connect(aboutAction, &QAction::triggered, this, &TrayTemperature::showAboutDialog);

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
    trayIconMenu->addAction(aboutAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(quitAction);

    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setContextMenu(trayIconMenu);
}

#endif

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

void TrayTemperature::on_buttonBox_main_clicked(QAbstractButton *button)
{
    if (ui->buttonBox_main->buttonRole(button) == QDialogButtonBox::AcceptRole) { // OK button
        // update settingsHolder with values from the configuration form
        qDebug() << "on_buttonBox_main_clicked AcceptRole";
        settingsHolder->temperatureUpdateFrequency = ui->spinBox_updateFrequency->value();
        settingsHolder->APIKey = ui->lineEdit_openWeatherApiKey->text();
        settingsHolder->temperatureDisplayUnits = ui->radio_metric->isChecked() ? "METRIC" : "IMPERIAL";
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
    else if (ui->buttonBox_main->buttonRole(button) == QDialogButtonBox::RejectRole) { // Cancel button
        qDebug() << "on_buttonBox_main_clicked RejectRole";
        //restore widgets' states from settingsHolder
        updateConfigDialogWidgets();
    }
    else if (ui->buttonBox_main->buttonRole(button) == QDialogButtonBox::ResetRole) { // Restore Defaults button
        qDebug() << "on_buttonBox_main_clicked ResetRole";
        //restore to "factory settings"
        defaultSettings();
        updateConfigDialogWidgets();
        fireAndRestartTimer();
    }
    else qFatal("unknown button pressed?");
}

void TrayTemperature::on_lineEdit_openWeatherApiKey_textChanged(const QString &text)
{
    // if the APIKey is empty, set its background to yellowish as an alert
    if (text.isEmpty()) {
        ui->lineEdit_openWeatherApiKey->setStyleSheet("background-color: rgb(255,255,127)");
    }
    else { // white
        ui->lineEdit_openWeatherApiKey->setStyleSheet("background-color: rgb(255,255,255)");
    }
}
