/*
    SPDX-FileCopyrightText: 2018 Volker Krause <vkrause@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "attributemodel.h"

#include <KItinerary/HtmlDocument>

#include <KLocalizedString>

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

    setHorizontalHeaderLabels({i18n("Attribute"), i18n("Value")});
}
