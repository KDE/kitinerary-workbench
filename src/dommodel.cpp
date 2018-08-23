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

#include "dommodel.h"

#include <KItinerary/HtmlDocument>

DOMModel::DOMModel(QObject *parent)
    : QStandardItemModel(parent)
{
}

DOMModel::~DOMModel() = default;

void DOMModel::setDocument(KItinerary::HtmlDocument *doc)
{
    clear();
    if (!doc)
        return;

    addNode(nullptr, doc->root());
    setHorizontalHeaderLabels({tr("Element"), tr("Content")});
}

void DOMModel::addNode(QStandardItem *parent, KItinerary::HtmlElement elem)
{
    auto i1 = new QStandardItem;
    i1->setText(elem.name());
    auto i2 = new QStandardItem;
    i2->setText(elem.content().left(200).replace(QLatin1Char('\n'), QLatin1Char(' ')));
    i2->setToolTip(elem.content());

    if (parent)
        parent->appendRow({i1, i2});
    else
        appendRow({i1, i2});

    for (auto child = elem.firstChild(); !child.isNull(); child = child.nextSibling())
        addNode(i1, child);
}

