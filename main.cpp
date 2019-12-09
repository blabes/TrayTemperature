/****************************************************************************

TrayTemperature - A Qt program to display the outdoor temperature
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

#include <QApplication>

#ifndef QT_NO_SYSTEMTRAYICON

#include <QMessageBox>
#include <QSystemTrayIcon>
#include "TrayTemperature.h"

int main(int argc, char *argv[])
{
    Q_INIT_RESOURCE(TrayTemperature);
    qSetMessagePattern("%{time} %{file}(%{line}): %{message}");
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    QApplication app(argc, argv);

    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        QMessageBox::critical(
            nullptr,
            QObject::tr("Systray"),
            QObject::tr("I couldn't detect any system tray on this system.")
        );
        return 1;
    }
    QApplication::setQuitOnLastWindowClosed(false);
    QApplication::setWindowIcon(QIcon(":/images/c-weather-cloudy-with-sun.svg"));
    TrayTemperature window;
    //window.show();
    return QApplication::exec();
}

#else

#include <QLabel>
#include <QDebug>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QString text("QSystemTrayIcon is not supported on this platform");

    QLabel *label = new QLabel(text);
    label->setWordWrap(true);

    label->show();
    qDebug() << text;

    app.exec();
}

#endif
