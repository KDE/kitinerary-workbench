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

#ifndef UIC9183WIDGET_H
#define UIC9183WIDGET_H

#include <QWidget>

#include <memory>

namespace Ui {
class Uic9183Widget;
}

namespace KItinerary {
class Uic9183Parser;
}

class Uic9183TicketLayoutModel;

class QStandardItemModel;


class Uic9183Widget : public QWidget
{
    Q_OBJECT
public:
    explicit Uic9183Widget(QWidget *parent = nullptr);
    ~Uic9183Widget();

    void clear();
    void setContent(const KItinerary::Uic9183Parser &p);

private:
    std::unique_ptr<Ui::Uic9183Widget> ui;

    QStandardItemModel *m_uic9183BlockModel;
    Uic9183TicketLayoutModel *m_ticketLayoutModel;
    QStandardItemModel *m_vendor0080BLModel;
};

#endif // UIC9183WIDGET_H
