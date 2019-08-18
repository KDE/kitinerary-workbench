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

#include "extractoreditorwidget.h"
#include "ui_extractoreditorwidget.h"

#include <KItinerary/Extractor>
#include <KItinerary/ExtractorRepository>

#include <KTextEditor/Document>
#include <KTextEditor/Editor>
#include <KTextEditor/View>

#include <QFile>

using namespace KItinerary;

ExtractorEditorWidget::ExtractorEditorWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ExtractorEditorWidget)
{
   ui->setupUi(this);

   connect(ui->extractorCombobox, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() {
        ExtractorRepository repo;
        const auto extId = ui->extractorCombobox->currentText();
        const auto extractor = repo.extractor(extId);
        m_scriptDoc->openUrl(QUrl::fromLocalFile(extractor.scriptFileName()));
   });
   connect(ui->reloadButton, &QPushButton::clicked, this, [this]() {
        ExtractorRepository repo;
        repo.reload();
        reloadExtractors();
   });

    auto editor = KTextEditor::Editor::instance();
    m_scriptDoc = editor->createDocument(nullptr);
    m_scriptDoc->setHighlightingMode(QStringLiteral("JavaScript"));
    auto view = m_scriptDoc->createView(nullptr);
    ui->topLayout->addWidget(view);
    reloadExtractors();
}

ExtractorEditorWidget::~ExtractorEditorWidget() = default;

void ExtractorEditorWidget::reloadExtractors()
{
    ui->extractorCombobox->clear();
    ExtractorRepository repo;
    for (const auto &ext : repo.allExtractors()) {
        ui->extractorCombobox->addItem(ext.name());
    }
}

void ExtractorEditorWidget::showExtractor(const QString &extractorId)
{
    const auto idx = ui->extractorCombobox->findText(extractorId);
    if (idx >= 0 && idx != ui->extractorCombobox->currentIndex()) {
        ui->extractorCombobox->setCurrentIndex(idx);
    }
}
