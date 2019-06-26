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

#ifndef DOMMODEL_H
#define DOMMODEL_H

#include <QStandardItemModel>

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

#endif // DOMMODEL_H
