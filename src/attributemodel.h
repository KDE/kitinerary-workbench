/*
    Copyright (C) 2018 Volker Krause <vkrause@kde.org>

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

#ifndef ATTRIBUTEMODEL_H
#define ATTRIBUTEMODEL_H

#include <QStandardItemModel>

namespace KItinerary {
class HtmlElement;
}

/** HTML attribute model. */
class AttributeModel : public QStandardItemModel
{
    Q_OBJECT
public:
    explicit AttributeModel(QObject *parent = nullptr);
    ~AttributeModel();

    void setElement(KItinerary::HtmlElement elem);
};

#endif // ATTRIBUTEMODEL_H
