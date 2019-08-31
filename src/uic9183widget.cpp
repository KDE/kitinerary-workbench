/*
    Copyright (C) 2019 Volker Krause <vkrause@kde.org>

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

#include "uic9183widget.h"
#include "ui_uic9183widget.h"
#include "uic9183ticketlayoutmodel.h"

#include <KItinerary/Uic9183Block>
#include <KItinerary/Uic9183Parser>
#include <KItinerary/Vendor0080Block>

#include <KLocalizedString>

#include <QClipboard>
#include <QMenu>
#include <QMimeData>
#include <QGuiApplication>
#include <QStandardItemModel>

using namespace KItinerary;

Uic9183Widget::Uic9183Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Uic9183Widget)
    , m_uic9183BlockModel(new QStandardItemModel(this))
    , m_ticketLayoutModel(new Uic9183TicketLayoutModel(this))
    , m_vendor0080BLModel(new QStandardItemModel(this))
{
    ui->setupUi(this);
    ui->splitter->setStretchFactor(0, 1);
    ui->splitter->setStretchFactor(1, 2);

    m_uic9183BlockModel->setHorizontalHeaderLabels({i18n("Block"), i18n("Version"), i18n("Size"), i18n("Content")});
    ui->blockView->setModel(m_uic9183BlockModel);
    ui->blockView->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    connect(ui->blockView, &QTreeView::customContextMenuRequested, this, [this](QPoint pos) {
        auto idx = ui->blockView->currentIndex();
        if (!idx.isValid())
            return;
        idx = idx.sibling(idx.row(), 3);

        QMenu menu;
        const auto copyContent = menu.addAction(i18n("Copy Content"));
        auto action = menu.exec(ui->blockView->viewport()->mapToGlobal(pos));
        if (action == copyContent) {
            auto md = new QMimeData;
            md->setData(QStringLiteral("application/octet-stream"), idx.data(Qt::UserRole).toByteArray());
            QGuiApplication::clipboard()->setMimeData(md);
        }
    });

    connect(ui->blockView->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this]() {
        const auto sel = ui->blockView->selectionModel()->selectedRows();
        if (sel.isEmpty()) {
            return;
        }
        const auto blockName = sel.at(0).data(Qt::DisplayRole).toString();
        if (blockName == QLatin1String("U_TLAY")) {
            ui->detailsStack->setCurrentWidget(ui->layoutPage);
        } else if (blockName == QLatin1String("0080BL")) {
            ui->detailsStack->setCurrentWidget(ui->vendor0080BLPage);
        } else {
            ui->detailsStack->setCurrentWidget(ui->noDetailsPage);
        }
    });

    ui->ticketLayoutTemplate->addItem(i18n("<no template>"), -1);
    const auto layoutTemplates = m_ticketLayoutModel->supportedTemplates();
    int i = 0;
    for (const auto &tpl : layoutTemplates) {
        ui->ticketLayoutTemplate->addItem(tpl, i++);
    }
    ui->ticketLayoutView->setModel(m_ticketLayoutModel);
    connect(ui->ticketLayoutTemplate, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() {
        m_ticketLayoutModel->setLayoutTemplate(ui->ticketLayoutTemplate->currentData().toInt());
    });
    QFontMetrics fm(font());
    const auto cellWidth = fm.boundingRect(QStringLiteral("m")).width() + 6;
    ui->ticketLayoutView->horizontalHeader()->setMinimumSectionSize(cellWidth);
    ui->ticketLayoutView->horizontalHeader()->setDefaultSectionSize(cellWidth);
    ui->ticketLayoutView->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    ui->ticketLayoutView->verticalHeader()->setMinimumSectionSize(fm.height());
    ui->ticketLayoutView->verticalHeader()->setMinimumSectionSize(fm.height());
    ui->ticketLayoutView->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    connect(ui->ticketLayoutView->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this]() {
        const auto sel = ui->ticketLayoutView->selectionModel()->selection();
        if (sel.isEmpty()) {
            ui->ticketLayoutSelection->clear();
        } else {
            const auto range = sel.at(0);
            ui->ticketLayoutSelection->setText(i18n("Row: %1 Column: %2 Width: %3 Height: %4", range.top(), range.left(), range.right() - range.left() + 1, range.bottom() - range.top() + 1));
        }
    });

    m_vendor0080BLModel->setHorizontalHeaderLabels({i18n("Name"), i18n("Size"), i18n("Content")});
    ui->vendor0080BLView->setModel(m_vendor0080BLModel);
    ui->vendor0080BLView->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
}

Uic9183Widget::~Uic9183Widget() = default;

void Uic9183Widget::clear()
{
    m_uic9183BlockModel->removeRows(0, m_uic9183BlockModel->rowCount());
    m_vendor0080BLModel->removeRows(0, m_vendor0080BLModel->rowCount());
}

void Uic9183Widget::setContent(const KItinerary::Uic9183Parser &p)
{
    m_ticketLayoutModel->setLayout(p.ticketLayout());
    auto idx = ui->ticketLayoutTemplate->findText(p.ticketLayout().type());
    ui->ticketLayoutTemplate->setCurrentIndex(std::max(idx, 0));

    auto block = p.firstBlock();
    while (!block.isNull()) {
        auto nameItem = new QStandardItem(QString::fromUtf8(block.name(), 6));
        auto versionItem = new QStandardItem(QString::number(block.version()));
        auto sizeItem = new QStandardItem(QString::number(block.contentSize()));
        auto contentItem = new QStandardItem;
        contentItem->setData(QByteArray(block.content(), block.contentSize()), Qt::UserRole);
        contentItem->setData(QString::fromUtf8(block.content(), block.contentSize()), Qt::DisplayRole);
        m_uic9183BlockModel->appendRow({nameItem, versionItem, sizeItem, contentItem});
        block = block.nextBlock();
    }

    const auto vendor0080BL = Vendor0080BLBlock(p.findBlock("0080BL"));
    if (vendor0080BL.isValid()) {
        auto sblock = vendor0080BL.firstBlock();
        while (!sblock.isNull()) {
            auto nameItem = new QStandardItem(QString::fromUtf8(sblock.id(), 3));
            auto sizeItem = new QStandardItem(QString::number(sblock.contentSize()));
            auto contentItem = new QStandardItem(sblock.toString());
            m_vendor0080BLModel->appendRow({nameItem, sizeItem, contentItem});
            sblock = sblock.nextBlock();
        }
    }
}
