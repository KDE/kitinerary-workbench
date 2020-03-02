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

#include <KColorScheme>
#include <KLocalizedString>

DOMModel::DOMModel(QObject *parent)
    : QStandardItemModel(parent)
{
}

DOMModel::~DOMModel() = default;

void DOMModel::setDocument(KItinerary::HtmlDocument *doc)
{
    m_highlightNodeSet.clear();
    clear();
    if (!doc)
        return;

    addNode(nullptr, doc->root());
    setHorizontalHeaderLabels({i18n("Element"), i18n("Content")});
}

QVariant DOMModel::data(const QModelIndex &index, int role) const
{
    if (role == Qt::BackgroundRole) {
        const auto elem = index.sibling(index.row(), 0).data(Qt::UserRole).value<KItinerary::HtmlElement>();
        if (std::find(m_highlightNodeSet.begin(), m_highlightNodeSet.end(), elem) != m_highlightNodeSet.end()) {
            return KColorScheme(QPalette::Normal).background(KColorScheme::PositiveBackground);
        }
    }

    return QStandardItemModel::data(index, role);
}

void DOMModel::setHighlightNodeSet(const QVariantList &nodeSet)
{
    m_highlightNodeSet.clear();
    m_highlightNodeSet.reserve(nodeSet.size());
    std::transform(nodeSet.begin(), nodeSet.end(), std::back_inserter(m_highlightNodeSet), [](const QVariant &v) { return v.value<KItinerary::HtmlElement>(); });
}

void DOMModel::addNode(QStandardItem *parent, KItinerary::HtmlElement elem)
{
    auto i1 = new QStandardItem;
    i1->setText(elem.name());
    i1->setData(QVariant::fromValue(elem), Qt::UserRole);

    if (elem.hasAttribute(QLatin1String("itemtype")) || elem.hasAttribute(QLatin1String("itemprop")) || elem.hasAttribute(QLatin1String("itemscope"))) {
        i1->setIcon(QIcon::fromTheme(QLatin1String("comment-symbolic")));
    } else if (elem.hasAttribute(QLatin1String("id")) || elem.hasAttribute(QLatin1String("class"))) {
        i1->setIcon(QIcon::fromTheme(QLatin1String("code-context")));
    }

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

DOMFilterModel::DOMFilterModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
}

DOMFilterModel::~DOMFilterModel() = default;

bool DOMFilterModel::filterAcceptsRow(int source_row, const QModelIndex& source_parent) const
{
    for (int i = 0; i < sourceModel()->columnCount(source_parent); ++i) {
        const auto str = sourceModel()->data(sourceModel()->index(source_row, i, source_parent), Qt::DisplayRole).toString();
        if (filterRegExp().indexIn(str) >= 0) {
            return true;
        }
    }

    return false;
}
