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

#include "attributemodel.h"

#include <KItinerary/HtmlDocument>

AttributeModel::AttributeModel(QObject *parent)
    : QStandardItemModel(parent)
{
}

AttributeModel::~AttributeModel() = default;

void AttributeModel::setElement(KItinerary::HtmlElement elem)
{
    clear();
    if (elem.isNull()) {
        return;
    }

    for (const auto &attr : elem.attributes()) {
        auto i1 = new QStandardItem;
        i1->setText(attr);
        auto i2 = new QStandardItem;
        i2->setText(elem.attribute(attr));
        i2->setToolTip(elem.attribute(attr));
        appendRow({i1, i2});
    }

    setHorizontalHeaderLabels({tr("Attribute"), tr("Value")});
}
