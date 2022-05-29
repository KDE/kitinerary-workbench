/*
    SPDX-FileCopyrightText: 2018 Volker Krause <vkrause@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
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
    m_document = doc;
    if (!doc)
        return;

    addNode(nullptr, doc->root());
    setHorizontalHeaderLabels({i18n("Element"), i18n("Content")});
}

KItinerary::HtmlDocument * DOMModel::document() const
{
    return m_document;
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
    i1->setFlags(i1->flags() & ~Qt::ItemIsEditable);

    if (elem.hasAttribute(QLatin1String("itemtype")) || elem.hasAttribute(QLatin1String("itemprop")) || elem.hasAttribute(QLatin1String("itemscope"))) {
        i1->setIcon(QIcon::fromTheme(QLatin1String("comment-symbolic")));
    } else if (elem.hasAttribute(QLatin1String("id")) || elem.hasAttribute(QLatin1String("class"))) {
        i1->setIcon(QIcon::fromTheme(QLatin1String("code-context")));
    }

    auto i2 = new QStandardItem;
    i2->setText(elem.content().left(200).replace(QLatin1Char('\n'), QLatin1Char(' ')));
    i2->setToolTip(elem.content());
    i2->setFlags(i2->flags() & ~Qt::ItemIsEditable);

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
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        if (filterRegExp().indexIn(str) >= 0) {
#else
        if (str.contains(filterRegularExpression())) {
#endif
            return true;
        }
    }

    return false;
}
