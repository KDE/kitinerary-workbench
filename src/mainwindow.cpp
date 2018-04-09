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

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <KItinerary/ExtractorEngine>
#include <KItinerary/ExtractorPreprocessor>
#include <KItinerary/ExtractorPostprocessor>
#include <KItinerary/JsonLdDocument>

#include <KMime/Message>

#include <KTextEditor/Document>
#include <KTextEditor/View>
#include <KTextEditor/Editor>

#include <QDebug>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setCentralWidget(ui->mainSplitter);

    auto editor = KTextEditor::Editor::instance();

    m_sourceDoc = editor->createDocument(nullptr);
    connect(m_sourceDoc, &KTextEditor::Document::textChanged, this, &MainWindow::sourceChanged);
    auto view = m_sourceDoc->createView(nullptr);
    ui->sourceTab->layout()->addWidget(view);

    m_preprocDoc = editor->createDocument(nullptr);
    view = m_preprocDoc->createView(nullptr);
    auto layout = new QHBoxLayout(ui->preprocTab);
    layout->addWidget(view);

    m_outputDoc = editor->createDocument(nullptr);
    m_outputDoc->setMode(QStringLiteral("JSON"));
    view = m_outputDoc->createView(nullptr);
    layout = new QHBoxLayout(ui->outputTab);
    layout->addWidget(view);

    m_postprocDoc = editor->createDocument(nullptr);
    m_postprocDoc->setMode(QStringLiteral("JSON"));
    view = m_postprocDoc->createView(nullptr);
    layout = new QHBoxLayout(ui->postprocTab);
    layout->addWidget(view);
}

MainWindow::~MainWindow() = default;

void MainWindow::sourceChanged()
{
    // TODO: select the right source type: plain, pkpass, pdf, html
    KItinerary::ExtractorPreprocessor preproc;
    preproc.preprocessHtml(m_sourceDoc->text());
    m_preprocDoc->setText(preproc.text());

    KMime::Message msg;
    msg.from()->fromUnicodeString(ui->senderLine->text(), "utf-8");
    const auto extractors = m_repo.extractorsForMessage(&msg);

    KItinerary::ExtractorEngine engine;
    engine.setText(preproc.text());
    QJsonArray data;
    for (const auto extractor : extractors) {
        engine.setExtractor(extractor);
        data = engine.extract();
        if (!data.isEmpty())
            break;
    }
    m_outputDoc->setText(QJsonDocument(data).toJson());

    KItinerary::ExtractorPostprocessor postproc;
    postproc.process(KItinerary::JsonLdDocument::fromJson(data));
    m_postprocDoc->setText(QJsonDocument(KItinerary::JsonLdDocument::toJson(postproc.result())).toJson());
}
