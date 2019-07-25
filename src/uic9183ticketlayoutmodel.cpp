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

#include "uic9183ticketlayoutmodel.h"

#include <QColor>
#include <QDebug>
#include <QSize>

enum {
    RCT2Width = 72,
    RCT2Height = 18
};

static const char rct2Layout[] =
"X             XXXX                                 X                   X"
"X             XXXX                                 X                   X"
"X   X    X  XXXXXX                                 X                   X"
"X                                                  X                   X"
"X     X     X                                      X     X     XXX     X"
"X     X     X                                      X     X     XXX     X"
"X     X     X                                      X     X     XXX     X"
"X     X     X                                      X     X     XXX     X"
"X                                                                      X"
"X                                                                      X"
"X                                                                      X"
"X                                                                      X"
"X                                                  XXXXXXXXXXXXXXXXXXXXX"
"X                                                  X                   X"
"X                                                  X                   X"
"X              X              X                    X                   X"
"XRRRRRRRRRRRX                 X                    X                   X"
"X           X                 X     X        X     X                   X"
;

static_assert(sizeof(rct2Layout) - 1 == (RCT2Width * RCT2Height), "RCT2 Layout template has wrong size");

Uic9183TicketLayoutModel::Uic9183TicketLayoutModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

Uic9183TicketLayoutModel::~Uic9183TicketLayoutModel() = default;

void Uic9183TicketLayoutModel::setLayout(const KItinerary::Uic9183TicketLayout &layout)
{
    beginResetModel();
    m_layout = layout;
    endResetModel();
}

int Uic9183TicketLayoutModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    if (m_layout.isValid() && m_layout.type() == QLatin1String("RCT2")) {
        return std::max<int>(RCT2Width, m_layout.size().width());
    }
    return m_layout.size().width();
}

int Uic9183TicketLayoutModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }

    if (m_layout.isValid() && m_layout.type() == QLatin1String("RCT2")) {
        return std::max<int>(RCT2Height, m_layout.size().height());
    }
    return m_layout.size().height();
}

QVariant Uic9183TicketLayoutModel::data(const QModelIndex& index, int role) const
{
    if (!m_layout.isValid() || !index.isValid()) {
        return {};
    }

    if (role == Qt::DisplayRole) {
        return m_layout.text(index.row(), index.column(), 1, 1);
    }
    if (role == Qt::BackgroundRole && m_layout.type() == QLatin1String("RCT2") && index.row() < RCT2Height && index.column() < RCT2Width) {
        const auto c = rct2Layout[index.row() * RCT2Width + index.column()];
        switch (c) {
            case 'X': return QColor(Qt::gray);
            case 'R': return QColor(Qt::red);
        }
    }

    return {};
}

QVariant Uic9183TicketLayoutModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
        return QString::number(section % 10);
    }
    if (role == Qt::DisplayRole && orientation == Qt::Vertical) {
        return QString::number(section);
    }

    return QAbstractTableModel::headerData(section, orientation, role);
}
