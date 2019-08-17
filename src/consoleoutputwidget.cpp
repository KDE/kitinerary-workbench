/*
    Copyright (C) 2019 Volker Krause <vkrause@kde.org>

    This program is free software; you can redistribute it and/or modify it
    under the terms of the GNU Library General Public License as published by
    the Free Software Foundation; either version 2 of the License, or (at your
    option) any later version.

    This program is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library General Public
    License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "consoleoutputwidget.h"
#include "ui_consoleoutputwidget.h"

#include <KLocalizedString>

#include <cstring>

static ConsoleOutputWidget *sConsoleOutput = nullptr;

void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    sConsoleOutput->handleMessage(type, context, msg);
}

ConsoleOutputWidget::ConsoleOutputWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ConsoleOutputWidget)
{
    ui->setupUi(this);
    sConsoleOutput = this;
    m_prevHandler = qInstallMessageHandler(messageHandler);
}

ConsoleOutputWidget::~ConsoleOutputWidget()
{
    qInstallMessageHandler(0);
    sConsoleOutput = nullptr;
}

void ConsoleOutputWidget::clear()
{
    ui->logView->clear();
}

void ConsoleOutputWidget::handleMessage(QtMsgType type, const QMessageLogContext& context, const QString& msg)
{
    m_prevHandler(type, context, msg);
    if (std::strcmp(context.category, "js") != 0) { // we only care for the script output
        return;
    }

    QString typeStr;
    switch (type) {
        case QtDebugMsg: typeStr = i18n("Debug"); break;
        case QtInfoMsg: typeStr = i18n("Info"); break;
        case QtWarningMsg: typeStr = i18n("Warning"); break;
        case QtCriticalMsg: typeStr = i18n("Critical"); break;
        case QtFatalMsg: typeStr = i18n("Fatal"); break;
    }
    ui->logView->appendPlainText(i18n("%1:%2:%3 [%4|%5] %6", context.file, context.function, context.line, context.category, typeStr, msg));
}
