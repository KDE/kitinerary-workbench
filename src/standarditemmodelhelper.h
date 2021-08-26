/*
    SPDX-FileCopyrightText: 2021 Volker Krause <vkrause@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef STANDARDITEMMODELHELPER_H
#define STANDARDITEMMODELHELPER_H

class QMetaObject;
class QStandardItem;
class QStandardItemModel;
class QString;
class QVariant;

/** Utility functions for producing QStandardItemModel content. */
namespace StandardItemModelHelper
{

void clearContent(QStandardItemModel *model);

QStandardItem* addEntry(const QString &key, const QString &value, QStandardItem *parent);

void fillFromGadget(const QVariant &value, QStandardItem *parent);
void fillFromGadget(const QMetaObject *mo, const void *gadget, QStandardItem *parent);
template <typename T>
inline void fillFromGadget(const T &value, QStandardItem *parent)
{
    return fillFromGadget(&T::staticMetaObject, &value, parent);
}

}

#endif // STANDARDITEMMODELHELPER_H
