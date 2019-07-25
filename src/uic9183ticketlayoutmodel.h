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

#ifndef UIC9183TICKETLAYOUTMODEL_H
#define UIC9183TICKETLAYOUTMODEL_H

#include <KItinerary/Uic9183TicketLayout>

#include <QAbstractTableModel>

/** Model showing a U_TLAY block of an UIC 918-3 ticket. */
class Uic9183TicketLayoutModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit Uic9183TicketLayoutModel(QObject *parent = nullptr);
    ~Uic9183TicketLayoutModel();

    void setLayout(const KItinerary::Uic9183TicketLayout &layout);

    int columnCount(const QModelIndex &parent) const override;
    int rowCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

private:
    KItinerary::Uic9183TicketLayout m_layout;
};

#endif // UIC9183TICKETLAYOUTMODEL_H
