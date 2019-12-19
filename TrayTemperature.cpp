/****************************************************************************

TrayTemperature.cpp - A Qt program to display the outdoor temperature
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

#include "ui_TrayTemperatureConfig.h"
#include "ui_TrayTemperatureAbout.h"
#include "TrayTemperature.h"
#include "TimedMessageBox.h"

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
    if (frequencySeconds < 60) qFatal("timer frequency too low: "); // sanity check that timer waits at least 1 minute

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
        qDebug() << "in closeEvent()";
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

void TrayTemperature::popupNetworkWarning(QNetworkReply *rep, const QString& msg) {
    trayIcon->setIcon(warningIcon);
    trayIcon->setToolTip(msg + tr(" request error"));

    // TimedMessageBox will replace the %1 placeholder every second with
    // the hh:mm:ss remaining before timeout
    const QString warningText = QString(tr("Problem requesting ") + msg +
                                        tr(" data\n"
                                           "Click 'Retry' to retry the request now\n"
                                           "Click 'Configure...' if you need to fix the configuration\n\n"
                                           "Retrying in %1..."));

    TimedMessageBox tmb;

    tmb.setTimeoutInSeconds(retryWaitSeconds);
    tmb.setText(warningText);
    tmb.setWindowTitle("TrayTemperature");
    QString ts = QDateTime::currentDateTime().toString();
    tmb.setDetailedText(QString(tr("Time: %1\nError: %2\nRequest: %3")).arg(ts, rep->errorString(), rep->url().toString()));
    // add "Configure..." and "Retry" buttons for convenience, and a standard "Cancel"
    QAbstractButton *retryButton = tmb.addButton(tr("Retry"), QMessageBox::AcceptRole);
    QAbstractButton *configureButton = tmb.addButton(tr("Configure..."), QMessageBox::AcceptRole);
    QAbstractButton *cancelButton = tmb.addButton(tr("Cancel"), QMessageBox::RejectRole);

    // TimedMessageBox will "click" this button if it times out,
    // and TimedMessageBox::timedOut() will return true
    tmb.setDefaultButton(static_cast<QPushButton*> (cancelButton));

    tmb.exec();
    qDebug() << "out of TimedMessageBox";
    qDebug() << "clicked: " << tmb.clickedButton()->text();

    if (tmb.clickedButton() == configureButton) {
        qDebug() << "configureButton";
        showConfigDialog();
    }
    else if (tmb.clickedButton() == retryButton) {
        qDebug() << "retryButton";
        retryWaitSeconds = minRetryWaitSeconds; // reset the retry timer if the user hits "Retry..."
        fireAndRestartTimer();
    }
    else if (tmb.clickedButton() == cancelButton) {
        qDebug() << "cancelButton";
        if (tmb.timedOut()) {
            qDebug() << "tmb timedOut";
            retryWaitSeconds = retryWaitSeconds * 2; // double time between retries
            if (retryWaitSeconds > 3600) retryWaitSeconds = 3600; // but max out an an hour
            fireAndRestartTimer();
        }
    }
}

void TrayTemperature::handleGeoLocationData(QNetworkReply *rep) {
    qDebug() << "handleGeoLocationData()";
    if (!rep) {
        trayIcon->setIcon(warningIcon);
        trayIcon->setToolTip(tr("Geolocation request error"));
        trayIcon->showMessage(tr("Geolocation request error"), tr("Reply was null"));
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
        popupNetworkWarning(rep, tr("geolocation"));
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
    connect(rep, &QNetworkReply::finished, this, [this, rep]() { handleWeatherNetworkData(rep); });
}

void TrayTemperature::handleWeatherNetworkData(QNetworkReply *rep){
    qDebug() << "handleWeatherNetworkData()";
    if (!rep) {
        trayIcon->setIcon(warningIcon);
        trayIcon->setToolTip(tr("Weather request error"));
        trayIcon->showMessage(tr("Weather network error"), tr("Reply was null"));
        qFatal("error with weather reply, rep was null");
    }

    if (!rep->error()) {
        QJsonDocument weatherJsonDoc = QJsonDocument::fromJson(rep->readAll());
        QVariantMap weatherVMap = weatherJsonDoc.object().toVariantMap();
        QVariantMap mainVMap = weatherVMap["main"].toMap();
        this->temperature = mainVMap["temp"].toDouble();
        if (settingsHolder->useManualLocation) {
            this->city=weatherVMap["name"].toString();
        }
        qDebug() << "temperature is: " << this->temperature;
        this->retryWaitSeconds = this->minRetryWaitSeconds; // reset the retry wait timer on success
        emit(temperatureRefreshed());
    }
    else {
        timer->stop();
        popupNetworkWarning(rep, tr("temperature"));
    }
}

void TrayTemperature::displayIcon() {

    QPixmap pix(systemTrayIconSize,systemTrayIconSize);
    pix.fill(settingsHolder->bgTemperatureColor);

    const QRect rectangle = QRect(0, 0, systemTrayIconSize, systemTrayIconSize);

    QPainter painter( &pix );
    painter.setPen(settingsHolder->temperatureColor);

    QString temperatureString = QString::number(qRound(this->temperature));
    if (settingsHolder->showDegreeSymbol) temperatureString += "°";

    QFont font = settingsHolder->temperatureFont;
    painter.setFont(font);

    if (settingsHolder->autoAdjustFontSize) { // user wants to autosize the font
        painter.setFont(adjustFontSizeForTrayIcon(font, temperatureString));
    }

    qDebug() << "temperatureString=" << temperatureString;
    painter.drawText(rectangle, Qt::AlignCenter|Qt::AlignVCenter, temperatureString);
    painter.end();
    QIcon myIcon(pix);

    QString units = settingsHolder->temperatureDisplayUnits == "METRIC" ? "°C" : "°F";
    QString toolTipString = "Temp";
    toolTipString += this->city.isEmpty() ? "" : tr(" in ") + this->city;
    toolTipString += QString(tr(" is %1%2")).arg(this->temperature).arg(units);

    trayIcon->setIcon(myIcon);
    trayIcon->setToolTip(toolTipString);
    trayIcon->show();

}

// Return a font based on the given font, but with its point size adjusted
// such that the given string will fit in a system tray icon (16x16 for Windows)
QFont TrayTemperature::adjustFontSizeForTrayIcon(QFont font, QString s) {
    QFont retFont = font;
    const QRect rectangle = QRect(0, 0, systemTrayIconSize, systemTrayIconSize);

    retFont.setPointSize(15); // start big, then decrement pointSize till the string fits into our rectangle
    QFontMetrics fm(retFont);
    QRect boundingRect = fm.boundingRect(rectangle, Qt::AlignCenter|Qt::AlignVCenter, s);
    while (retFont.pointSize() > 1 && (boundingRect.height() > systemTrayIconSize || boundingRect.width() > systemTrayIconSize)) {
        retFont.setPointSize(retFont.pointSize() - 1);
        QFontMetrics fm(retFont);
        boundingRect = fm.boundingRect(rectangle, Qt::AlignCenter|Qt::AlignVCenter, s);
        //qDebug() << "boundingRect: " << boundingRect;
        //qDebug() << "autoresized font: " << retFont.toString();
    }
    qDebug() << "adjusted to a font size of " << retFont.pointSize();
    return retFont;
}

void TrayTemperature::updateTemperatureDisplaySampleLabel() {
    // TODO: use stylesheet?
    // TODO: do something about the label being invisible when FG/BG colors are the same

    QString temperatureString = settingsHolder->showDegreeSymbol ? "42°" : "42";
    QFont font = settingsHolder->temperatureFont;
    if (settingsHolder->autoAdjustFontSize) {
        font = adjustFontSizeForTrayIcon(font, temperatureString);
    }
    ui->label_sampleDisplay->setFont(font);
    ui->label_sampleDisplay->setText(temperatureString);

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
            (font.bold() ? TrayTemperature::tr("bold ") : "") +
            (font.italic() ? TrayTemperature::tr("italic ") : "") +
            (font.underline() ? TrayTemperature::tr("underline ") : "");
}

void TrayTemperature::highlightIfEmpty(QLineEdit *e) {
    // set background to yellowish if text is empty, white otherwise
    QString colorSpec = e->text().isEmpty() ? "rgb(255,255,127)" : "rgb(255,255,255)";
    e->setStyleSheet("background-color: " + colorSpec);
}

// make all the UI widgets reflect the values stored in settingsHolder
void TrayTemperature::updateConfigDialogWidgets() {
    qDebug() << "updateConfigDialogWidgets()";
    ui->radio_imperial->setChecked(settingsHolder->temperatureDisplayUnits == "IMPERIAL");
    ui->radio_metric->setChecked(settingsHolder->temperatureDisplayUnits == "METRIC");

    ui->label_font->setText(fontDisplayName(settingsHolder->temperatureFont));
    ui->label_fgColor->setText(settingsHolder->temperatureColor.name());
    ui->label_bgColor->setText(settingsHolder->bgTemperatureColor.name());

    ui->lineEdit_openWeatherApiKey->setText(settingsHolder->APIKey);
    highlightIfEmpty(ui->lineEdit_openWeatherApiKey);

    ui->spinBox_updateFrequency->setValue(settingsHolder->temperatureUpdateFrequency);
    ui->checkBox_transparentBg->setChecked(settingsHolder->bgTemperatureColorTransparent);
    ui->checkBox_showDegreeSymbol->setChecked(settingsHolder->showDegreeSymbol);
    ui->checkBox_autoSizeFont->setChecked(settingsHolder->autoAdjustFontSize);
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
    this->holdMyFont = settingsHolder->temperatureFont;
    settingsHolder->temperatureColor = settings->value("temperatureColor").value<QColor>();
    settingsHolder->bgTemperatureColor = settings->value("bgTemperatureColor").value<QColor>();
    settingsHolder->bgTemperatureColorTransparent = settings->value("bgTemperatureColorTransparent").toBool();
    settingsHolder->bgTemperatureColor.setAlpha(settingsHolder->bgTemperatureColorTransparent ? 0 : 255);

    settingsHolder->useManualLocation = settings->value("useManualLocation").toBool();
    settingsHolder->manualLat = settings->value("manualLat").toDouble();
    settingsHolder->manualLon = settings->value("manualLon").toDouble();

    settingsHolder->autoAdjustFontSize = settings->value("autoAdjustFontSize").toBool();
    settingsHolder->showDegreeSymbol = settings->value("showDegreeSymbol").toBool();
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

    settings->setValue("autoAdjustFontSize", settingsHolder->autoAdjustFontSize);
    settings->setValue("showDegreeSymbol",settingsHolder->showDegreeSymbol);
}

void TrayTemperature::defaultSettings() {
    qDebug() << "defaultSettings()";

    settingsHolder->temperatureUpdateFrequency = settingsHolder->temperatureUpdateFrequencyDefault;
    settingsHolder->temperatureDisplayUnits = settingsHolder->temperatureDisplayUnitsDefault;
    settingsHolder->temperatureFont = settingsHolder->temperatureFontDefault;
    this->holdMyFont = settingsHolder->temperatureFont;
    settingsHolder->temperatureColor = settingsHolder->temperatureColorDefault;
    settingsHolder->bgTemperatureColor = settingsHolder->bgTemperatureColorDefault;
    settingsHolder->bgTemperatureColorTransparent = settingsHolder->bgTemperatureColorTransparentDefault;
    settingsHolder->bgTemperatureColor.setAlpha(settingsHolder->bgTemperatureColorTransparent ? 0 : 255);
    settingsHolder->useManualLocation = settingsHolder->useManualLocationDefault;
    settingsHolder->manualLat = settingsHolder->manualLatDefault;
    settingsHolder->manualLon = settingsHolder->manualLonDefault;
    settingsHolder->autoAdjustFontSize = settingsHolder->autoAdjustFontSizeDefault;
    settingsHolder->showDegreeSymbol = settingsHolder->showDegreeSymbolDefault;
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
    auto titleAction = new QWidgetAction(trayIconMenu);
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
    QFont font = QFontDialog::getFont(&ok, this->holdMyFont);
    QFont possiblyAdjustedFont = font;
    this->holdMyFont = font;
    if (ok) {
        qDebug() << "font is now: " << font;
        ui->label_font->setText(fontDisplayName(font));
        if (ui->checkBox_autoSizeFont->isChecked()) {
            possiblyAdjustedFont = adjustFontSizeForTrayIcon(font, ui->label_sampleDisplay->text());
        }
        ui->label_sampleDisplay->setFont(possiblyAdjustedFont);
    } else {
        qDebug() << "font is still: " << font;
    }
}

void TrayTemperature::on_button_fgColor_clicked()
{
    const QColor color = QColorDialog::getColor(settingsHolder->temperatureColor);
    if (color.isValid()) {
        qDebug() << "color is now" << color;
        ui->label_fgColor->setText(color.name());

        QPalette palette = ui->label_sampleDisplay->palette();
        palette.setColor(ui->label_sampleDisplay->foregroundRole(), color);
        ui->label_sampleDisplay->setPalette(palette);
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
    ui->spinBox_lat->setDisabled(true);
    ui->spinBox_lon->setDisabled(true);
}

void TrayTemperature::on_radio_useManualLocation_clicked()
{
    qDebug() << "on_radio_useManualLocation_clicked()";
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
        this->holdMyFont = settingsHolder->temperatureFont;
        settingsHolder->temperatureColor = ui->label_sampleDisplay->palette().color(ui->label_sampleDisplay->foregroundRole());
        settingsHolder->bgTemperatureColor = ui->label_sampleDisplay->palette().color(ui->label_sampleDisplay->backgroundRole());
        settingsHolder->useManualLocation = ui->radio_useManualLocation->isChecked();
        settingsHolder->manualLat = ui->spinBox_lat->value();
        settingsHolder->manualLon = ui->spinBox_lon->value();
        settingsHolder->showDegreeSymbol = ui->checkBox_showDegreeSymbol->isChecked();
        settingsHolder->autoAdjustFontSize = ui->checkBox_autoSizeFont->isChecked();

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
    // if the APIKey becomes empty, set its background to yellowish as an alert
    Q_UNUSED(text)
    highlightIfEmpty(ui->lineEdit_openWeatherApiKey);
}

void TrayTemperature::on_checkBox_showDegreeSymbol_stateChanged(int state)
{
    qDebug() << QString("on_checkBox_showDegreeSymbol_stateChanged(%1)").arg(state);
    ui->label_sampleDisplay->setText(state ? "42°" : "42");
    if (ui->checkBox_autoSizeFont->isChecked()) {
        QFont font = adjustFontSizeForTrayIcon(ui->label_sampleDisplay->font(), ui->label_sampleDisplay->text());
        ui->label_sampleDisplay->setFont(font);
    }
}

void TrayTemperature::on_checkBox_autoSizeFont_stateChanged(int state)
{
    qDebug() << QString("on_checkBox_autoSizeFont_stateChanged(%1)").arg(state);
    if (state) {
        QFont font = adjustFontSizeForTrayIcon(ui->label_sampleDisplay->font(), ui->label_sampleDisplay->text());
        ui->label_sampleDisplay->setFont(font);
    }
    else {
        ui->label_sampleDisplay->setFont(this->holdMyFont);
    }
}

void TrayTemperature::testWeatherNetworkData(QNetworkReply *rep) {
    if (!rep) {
        trayIcon->setIcon(warningIcon);
        trayIcon->setToolTip(tr("Weather request test error"));
        trayIcon->showMessage(tr("Weather network test error"), tr("Reply was null"));
        qFatal("error with weather API test reply, rep was null");
    }

    QMessageBox msgBox;

    msgBox.setWindowTitle("TrayTemperature");
    QString ts = QDateTime::currentDateTime().toString();

    if (!rep->error()) {
        msgBox.setText("API Key test succeeded!");
        msgBox.setIcon(QMessageBox::Information);
        msgBox.setDetailedText(QString(tr("Time: %1\nRequest: %2")).arg(ts, rep->url().toString()));
    }
    else {
        msgBox.setText("API Key test failed.");
        msgBox.setIcon(QMessageBox::Critical);
        msgBox.setDetailedText(QString(tr("Time: %1\nError: %2\nRequest: %3")).arg(ts, rep->errorString(), rep->url().toString()));
    }

    msgBox.exec();

}

void TrayTemperature::on_pushButton_testApiKey_clicked()
{
    qDebug() << "on_pushButton_testApiKey_clicked()";
    QUrl url("http://api.openweathermap.org/data/2.5/weather");

    QUrlQuery query;

    query.addQueryItem("lat", "0");
    query.addQueryItem("lon", "0");
    query.addQueryItem("appid", ui->lineEdit_openWeatherApiKey->text());
    url.setQuery(query);
    qDebug() << "submitting request: " << url;

    QNetworkReply *rep = nam->get(QNetworkRequest(url));
    // connect up the signal right away
    connect(rep, &QNetworkReply::finished, this, [this, rep]() { testWeatherNetworkData(rep); });
}
