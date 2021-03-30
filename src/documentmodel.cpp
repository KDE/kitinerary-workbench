/*
    SPDX-FileCopyrightText: 2021 Volker Krause <vkrause@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "documentmodel.h"

#include <KItinerary/ExtractorDocumentNode>

#include <KLocalizedString>

#include <QIcon>
#include <QMimeDatabase>

using namespace KItinerary;

DocumentModel::DocumentModel(QObject *parent)
    : QStandardItemModel(parent)
{
}

void DocumentModel::setRootNode(const ExtractorDocumentNode &root)
{
    clear();
    addNode(root, nullptr);
    setHorizontalHeaderLabels({i18n("Type"), i18n("Context Time")});
}

void DocumentModel::addNode(const ExtractorDocumentNode& node, QStandardItem *parent)
{
    auto i1 = new QStandardItem;
    i1->setText(node.mimeType());
    i1->setData(QVariant::fromValue(node), Qt::UserRole);
    if (!node.location().isNull()) {
        i1->setToolTip(i18n("Location: %1", node.location().toString()));
    }

    QMimeDatabase db;
    const auto mt = db.mimeTypeForName(node.mimeType());
    if (mt.isValid()) {
        i1->setIcon(QIcon::fromTheme(mt.iconName(), QIcon::fromTheme(mt.genericIconName())));
    }

    auto i2 = new QStandardItem;
    i2->setText(node.contextDateTime().toString(Qt::ISODate));

    if (parent)
        parent->appendRow({i1, i2});
    else
        appendRow({i1, i2});

    for (const auto &child : node.childNodes())
        addNode(child, i1);
}
