/*
    SPDX-FileCopyrightText: 2018 Volker Krause <vkrause@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
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
