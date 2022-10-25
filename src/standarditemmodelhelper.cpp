/*
    SPDX-FileCopyrightText: 2021 Volker Krause <vkrause@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "standarditemmodelhelper.h"

#include <QMetaProperty>
#include <QSequentialIterable>
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
        auto item = addEntry(QString::fromUtf8(prop.name()), value.toString().isEmpty() ? QString::fromUtf8(value.typeName()) : value.toString(), parent);
        if (const auto childMo = QMetaType::metaObjectForType(value.userType())) {
            fillFromGadget(childMo, value.constData(), item);
        } else if (value.canConvert<QVariantList>() && value.type() != QVariant::String && value.type() != QVariant::ByteArray) {
            auto iterable = value.value<QSequentialIterable>();
            int idx = 0;
            for (const QVariant &v : iterable) {
                auto arrayItem = addEntry(QString::number(idx++), v.toString(), item);
                if (const auto childMo = QMetaType::metaObjectForType(v.userType())) {
                    fillFromGadget(childMo, v.constData(), arrayItem);
                }
            }
        }
    }
}

QString StandardItemModelHelper::dataToHex(const uint8_t *data, int size, int offset)
{
    return QString::fromUtf8(QByteArray((const char*)data + offset, size - offset).toHex());
}
