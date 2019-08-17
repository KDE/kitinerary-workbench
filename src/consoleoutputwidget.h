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

#ifndef CONSOLEOUTPUTWIDGET_H
#define CONSOLEOUTPUTWIDGET_H

#include <QWidget>

#include <memory>

namespace Ui {
class ConsoleOutputWidget;
}

class ConsoleOutputWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ConsoleOutputWidget(QWidget *parent = nullptr);
    ~ConsoleOutputWidget();

    void clear();

    void handleMessage(QtMsgType type, const QMessageLogContext &context, const QString &msg);

private:
    std::unique_ptr<Ui::ConsoleOutputWidget> ui;
    QtMessageHandler m_prevHandler = nullptr;
};

#endif // CONSOLEOUTPUTWIDGET_H