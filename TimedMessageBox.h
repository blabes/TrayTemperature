/****************************************************************************

TimedMessageBox.h

Adapted from https://gist.github.com/Blubbz0r/d7c4d81f47098b4e524d, a
QMessageBox including a countdown value that is displayed to the user.
When the countdown reaches 0, the message box automatically 'clicks' on
the default button and timedOut() will return true.

Sample Usage:

    TimedMessageBox tmb;

    tmb.setTimeoutInSeconds(25);
    tmb.setText("Will retry in %1");
    tmb.setSecondsFormat("mm:ss");
    tmb.setWindowTitle("My Application");
    QAbstractButton *retryButton = tmb.addButton(tr("Retry"), QMessageBox::AcceptRole);
    QAbstractButton *cancelButton = tmb.addButton(tr("Cancel"), QMessageBox::RejectRole);

    tmb.setDefaultButton(static_cast<QPushButton*> (retryButton));

    tmb.exec();

    if (tmb.clickedButton() == cancelButton) {
        qDebug() << "cancelButton";
    }
    else if (tmb.clickedButton() == retryButton) {
        qDebug() << "retryButton";
        if (tmb.timedOut()) {
            qDebug() << "timedOut";
        }
   }

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

#ifndef TIMEDMESSAGEBOX_H
#define TIMEDMESSAGEBOX_H

#include <QMessageBox>
#include <QPushButton>
#include <QTimer>
#include <QDebug>
#include <QDateTime>

class TimedMessageBox : public QMessageBox
{
    Q_OBJECT

public:
    TimedMessageBox(
            int             timeoutInSeconds = 10,
            const QString&  format           = "hh:mm:ss",
            QPushButton*    defaultButton    = nullptr,
            Icon            icon             = QMessageBox::Warning,
            const QString&  title            = "Warning",
            const QString&  text             = "Warning",
            StandardButtons buttons          = QMessageBox::NoButton,
            QWidget*        parent           = nullptr,
            Qt::WindowFlags flags            = Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint
            ) :
        QMessageBox(icon, title, text, buttons, parent, flags),
        m_timeoutInSeconds(timeoutInSeconds),
        m_text(text),
        m_timedOut(false),
        m_secondsFormat(format)
    {
        setDefaultButton(defaultButton);
        connect(&m_timer, &QTimer::timeout, this, &TimedMessageBox::updateTimeout);
        m_timer.setInterval(1000); // 1 second
    }

    void setTimeoutInSeconds(int seconds) {
        m_timeoutInSeconds = seconds;
    }

    void setText(const QString &text) {
        m_text = text;
    }

    void setSecondsFormat(const QString &format) {
        m_secondsFormat = format;
    }

    bool timedOut() {
        return m_timedOut;
    }

    void showEvent(QShowEvent* event) override {
        QMessageBox::showEvent(event);
        updateTimeout();
        m_timer.start();
    }

private slots:
    void updateTimeout() {
        qDebug() << "TimedMessageBox tick..." << m_timeoutInSeconds;
        if (--m_timeoutInSeconds >= 0) {
            QString displaySecs = QDateTime::fromSecsSinceEpoch(m_timeoutInSeconds, Qt::UTC).toString(m_secondsFormat);
            QMessageBox::setText(m_text.arg(displaySecs));
        }
        else {
            m_timer.stop();
            m_timedOut = true;

            if (!defaultButton()) {
                qCritical("No valid default button");
                return;
            }

            defaultButton()->animateClick();
        }
    }

private:
    int m_timeoutInSeconds;
    QString m_text;
    QTimer m_timer;
    bool m_timedOut;
    QString m_secondsFormat;
};
#endif
