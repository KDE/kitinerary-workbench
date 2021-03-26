/*
    SPDX-FileCopyrightText: 2018 Volker Krause <vkrause@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef DOMMODEL_H
#define DOMMODEL_H

#include <QStandardItemModel>
#include <QSortFilterProxyModel>

#include <vector>

namespace KItinerary {
class HtmlDocument;
class HtmlElement;
}

/** DOM model for a KItinerary::HtmlDocument. */
class DOMModel : public QStandardItemModel
{
    Q_OBJECT
public:
    explicit DOMModel(QObject *parent = nullptr);
    ~DOMModel();

    QVariant data(const QModelIndex &index, int role) const override;

    void setDocument(KItinerary::HtmlDocument *doc);

    void setHighlightNodeSet(const QVariantList &nodeSet);

private:
    void addNode(QStandardItem *parent, KItinerary::HtmlElement elem);

    std::vector<KItinerary::HtmlElement> m_highlightNodeSet;
};

/** DOM multi-column aware filter model for searching. */
class DOMFilterModel : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    explicit DOMFilterModel(QObject *parent = nullptr);
    ~DOMFilterModel();

protected:
    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;
};

#endif // DOMMODEL_H
