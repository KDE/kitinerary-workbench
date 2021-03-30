/*
    SPDX-FileCopyrightText: 2021 Volker Krause <vkrause@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef DOCUMENTMODEL_H
#define DOCUMENTMODEL_H

# include <QStandardItemModel>

namespace KItinerary {
class ExtractorDocumentNode;
}

/** Extractor document node model. */
class DocumentModel : public QStandardItemModel
{
    Q_OBJECT
public:
    explicit DocumentModel(QObject *parent = nullptr);

    void setRootNode(const KItinerary::ExtractorDocumentNode &root);

private:
    void addNode(const KItinerary::ExtractorDocumentNode &node, QStandardItem *parent);
};

#endif // DOCUMENTMODEL_H
