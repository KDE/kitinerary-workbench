/*
    SPDX-FileCopyrightText: 2021 Volker Krause <vkrause@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "standarditemmodelhelper.h"

#include <QMetaProperty>
#include <QStandardItem>

void StandardItemModelHelper::clearContent(QStandardItemModel *model)
{
    model->removeRows(0, model->rowCount());
}

QStandardItem* StandardItemModelHelper::addEntry(const QString &key, const QString &value, QStandardItem *parent)
{
    auto item1 = new QStandardItem;
    item1->setText(key);
    item1->setFlags(item1->flags() & ~Qt::ItemIsEditable);
    auto item2 = new QStandardItem;
    item2->setText(value);
    parent->appendRow({item1, item2});
    return item1;
}

void StandardItemModelHelper::fillFromGadget(const QVariant &value, QStandardItem *parent)
{
    fillFromGadget(QMetaType(value.userType()).metaObject(), value.constData(), parent);
}

void StandardItemModelHelper::fillFromGadget(const QMetaObject *mo, const void *gadget, QStandardItem *parent)
{
    if (!gadget || !mo) {
        return;
    }
    for (auto i = 0; i < mo->propertyCount(); ++i) {
        const auto prop = mo->property(i);
        if (!prop.isStored()) {
            continue;
        }
        const auto value = prop.readOnGadget(gadget);
        addEntry(QString::fromUtf8(prop.name()), value.toString(), parent);
    }
}
