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
#include <KItinerary/IataBcbpParser>
#include <KItinerary/JsonLdDocument>
#include <KItinerary/PdfDocument>
#include <KItinerary/StructuredDataExtractor>

#include <KPkPass/Pass>

#include <KMime/Message>

#include <KTextEditor/Document>
#include <KTextEditor/View>
#include <KTextEditor/Editor>

#include <QDebug>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QStandardItemModel>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_imageModel(new QStandardItemModel(this))
{
    ui->setupUi(this);
    ui->contextDate->setDateTime(QDateTime(QDate::currentDate(), QTime()));
    setCentralWidget(ui->mainSplitter);

    connect(ui->typeBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::typeChanged);
    connect(ui->typeBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::sourceChanged);
    connect(ui->senderBox, &QComboBox::currentTextChanged, this, &MainWindow::sourceChanged);
    connect(ui->contextDate, &QDateTimeEdit::dateTimeChanged, this, &MainWindow::sourceChanged);
    connect(ui->fileRequester, &KUrlRequester::textChanged, this, &MainWindow::urlChanged);

    auto editor = KTextEditor::Editor::instance();

    m_sourceDoc = editor->createDocument(nullptr);
    connect(m_sourceDoc, &KTextEditor::Document::textChanged, this, &MainWindow::sourceChanged);
    m_sourceView = m_sourceDoc->createView(nullptr);
    ui->sourceTab->layout()->addWidget(m_sourceView);

    m_preprocDoc = editor->createDocument(nullptr);
    auto view = m_preprocDoc->createView(nullptr);
    auto layout = new QHBoxLayout(ui->preprocTab);
    layout->addWidget(view);

    m_imageModel->setHorizontalHeaderLabels({tr("Image")});
    ui->imageView->setModel(m_imageModel);

    m_structuredDoc = editor->createDocument(nullptr);
    m_structuredDoc->setMode(QStringLiteral("JSON"));
    view = m_structuredDoc->createView(nullptr);
    layout = new QHBoxLayout(ui->structuredTab);
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

    typeChanged();

    QSettings settings;
    settings.beginGroup(QLatin1String("MainWindow"));
    restoreGeometry(settings.value(QLatin1String("Geometry")).toByteArray());
    restoreState(settings.value(QLatin1String("State")).toByteArray());
    settings.endGroup();
    settings.beginGroup(QLatin1String("SenderHistory"));
    ui->senderBox->addItems(settings.value(QLatin1String("History")).toStringList());
    ui->senderBox->setCurrentText(QString());
}

MainWindow::~MainWindow()
{
    QSettings settings;

    settings.beginGroup(QLatin1String("SenderHistory"));
    QStringList history;
    history.reserve(ui->senderBox->count());
    for (int i = 0; i < ui->senderBox->count(); ++i)
        history.push_back(ui->senderBox->itemText(i));
    settings.setValue(QLatin1String("History"), history);
    settings.endGroup();

    settings.beginGroup(QLatin1String("MainWindow"));
    settings.setValue(QLatin1String("Geometry"), saveGeometry());
    settings.setValue(QLatin1String("State"), saveState());
}

void MainWindow::typeChanged()
{
    ui->inputTabWidget->setTabEnabled(1, false);
    ui->inputTabWidget->setTabEnabled(2, false);
    ui->outputTabWidget->setTabEnabled(1, true);
    switch (ui->typeBox->currentIndex()) {
        case PlainText:
        case IataBcbp:
            m_sourceDoc->setMode(QStringLiteral("Normal"));
            m_sourceView->show();
            ui->outputTabWidget->setTabEnabled(0, false);
            break;
        case Html:
            m_sourceDoc->setMode(QStringLiteral("HTML"));
            m_sourceView->show();
            ui->inputTabWidget->setTabEnabled(1, true);
            ui->outputTabWidget->setTabEnabled(0, true);
            break;
        case Pdf:
            ui->inputTabWidget->setTabEnabled(1, true);
            ui->inputTabWidget->setTabEnabled(2, true);
            m_sourceView->hide();
            ui->outputTabWidget->setTabEnabled(0, false);
            break;
        case PkPass:
            ui->inputTabWidget->setTabEnabled(1, true);
            m_sourceView->hide();
            ui->outputTabWidget->setTabEnabled(0, false);
            break;
        case JsonLd:
            m_sourceDoc->setMode(QStringLiteral("JSON"));
            m_sourceView->show();
            ui->outputTabWidget->setTabEnabled(0, false);
            ui->outputTabWidget->setTabEnabled(1, false);
            break;
    }
}

void MainWindow::sourceChanged()
{
    m_imageModel->removeRows(0, m_imageModel->rowCount());
    using namespace KItinerary;

    QJsonArray data;
    if (ui->typeBox->currentIndex() == IataBcbp) {
        const auto bp = IataBcbpParser::parse(m_sourceDoc->text(), ui->contextDate->date());
        data = JsonLdDocument::toJson({bp});
    } else if (ui->typeBox->currentIndex() == PkPass) {
        const auto extractors = m_repo.extractorsForPass(m_pkpass.get());
        ExtractorEngine engine;
        engine.setSenderDate(ui->contextDate->dateTime());
        engine.setPass(m_pkpass.get());
        for (const auto extractor : extractors) {
            engine.setExtractor(extractor);
            data = engine.extract();
            if (!data.isEmpty())
                break;
        }
    } else if (ui->typeBox->currentIndex() == JsonLd) {
        const auto doc = QJsonDocument::fromJson(m_sourceDoc->text().toUtf8());
        if (doc.isArray())
            data = doc.array();
        else if (doc.isObject())
            data = {doc.object()};
    } else {
        ExtractorPreprocessor preproc;
        ExtractorEngine engine;
        engine.setSenderDate(ui->contextDate->dateTime());

        if (ui->typeBox->currentIndex() == PlainText) {
            preproc.preprocessPlainText(m_sourceDoc->text());
            engine.setText(preproc.text());
            m_preprocDoc->setText(preproc.text());
        } else if (ui->typeBox->currentIndex() == Html) {
            preproc.preprocessHtml(m_sourceDoc->text());
            engine.setText(preproc.text());
            m_preprocDoc->setText(preproc.text());
        } else if (ui->typeBox->currentIndex() == Pdf && m_pdfDoc) {
            engine.setPdfDocument(m_pdfDoc.get());
            m_preprocDoc->setText(m_pdfDoc->text());

            for (int i = 0; i < m_pdfDoc->imageCount(); ++i) {
                auto item = new QStandardItem;
                item->setData(m_pdfDoc->image(i), Qt::DecorationRole);
                m_imageModel->appendRow(item);
            }
        }

        KMime::Message msg;
        msg.from()->fromUnicodeString(ui->senderBox->currentText(), "utf-8");
        const auto extractors = m_repo.extractorsForMessage(&msg);

        for (const auto extractor : extractors) {
            engine.setExtractor(extractor);
            data = engine.extract();
            if (!data.isEmpty())
                break;
        }
    }

    m_outputDoc->setText(QJsonDocument(data).toJson());

    QJsonArray structured;
    if (ui->typeBox->currentIndex() == Html) {
        StructuredDataExtractor extractor;
        extractor.parse(m_sourceDoc->text());
        structured = extractor.data();
        m_structuredDoc->setText(QJsonDocument(structured).toJson());
    }

    ExtractorPostprocessor postproc;
    postproc.setContextDate(ui->contextDate->dateTime());
    postproc.process(JsonLdDocument::fromJson(structured.isEmpty() ? data : structured));
    m_postprocDoc->setText(QJsonDocument(JsonLdDocument::toJson(postproc.result())).toJson());
}

void MainWindow::urlChanged()
{
    const auto url = ui->fileRequester->url();
    if (!url.isValid())
        return;

    if (url.toString().endsWith(QLatin1String(".pkpass"))) {
        m_pkpass.reset(KPkPass::Pass::fromFile(url.toLocalFile()));
        ui->typeBox->setCurrentIndex(PkPass);
        sourceChanged();
    } else if (url.toString().endsWith(QLatin1String(".pdf"))) {
        QFile f(url.toLocalFile());
        f.open(QFile::ReadOnly);
        m_pdfDoc.reset(KItinerary::PdfDocument::fromData(f.readAll()));
        ui->typeBox->setCurrentIndex(Pdf);
        sourceChanged();
    } else {
        m_sourceDoc->openUrl(url);
    }
}
