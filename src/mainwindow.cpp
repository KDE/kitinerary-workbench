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
#include "attributemodel.h"
#include "dommodel.h"

#include <KItinerary/BarcodeDecoder>
#include <KItinerary/ExtractorEngine>
#include <KItinerary/ExtractorPostprocessor>
#include <KItinerary/HtmlDocument>
#include <KItinerary/IataBcbpParser>
#include <KItinerary/JsonLdDocument>
#include <KItinerary/PdfDocument>
#include <KItinerary/Uic9183Parser>

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
#include <QMenu>
#include <QSettings>
#include <QStandardItemModel>
#include <QTextCodec>
#include <QToolBar>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_imageModel(new QStandardItemModel(this))
    , m_domModel(new DOMModel(this))
    , m_attrModel(new AttributeModel(this))
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
    connect(ui->imageView, &QWidget::customContextMenuRequested, this, &MainWindow::imageContextMenu);

    ui->domView->setModel(m_domModel);
    ui->domView->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->attributeView->setModel(m_attrModel);
    ui->attributeView->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    connect(ui->domView->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this](const QItemSelection &selection) {
        auto idx = selection.value(0).topLeft();
        m_attrModel->setElement(idx.data(Qt::UserRole).value<KItinerary::HtmlElement>());

        QString path;
        idx = idx.sibling(idx.row(), 0);
        while (idx.isValid()) {
            path.prepend(QLatin1Char('/') + idx.data(Qt::DisplayRole).toString());
            idx = idx.parent();
        }
        ui->domPath->setText(path);
    });
    ui->domSplitter->setStretchFactor(0, 5);
    ui->domSplitter->setStretchFactor(1, 1);

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

    ui->actionReload->setShortcut(QKeySequence::Refresh);
    ui->actionQuit->setShortcut(QKeySequence::Quit);
    connect(ui->actionReload, &QAction::triggered, this, &MainWindow::sourceChanged);
    connect(ui->actionQuit, &QAction::triggered, QCoreApplication::instance(), &QCoreApplication::quit);
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

void MainWindow::openFile(const QString &file)
{
    ui->fileRequester->setText(file);
}

void MainWindow::typeChanged()
{
    ui->inputTabWidget->setTabEnabled(1, false);
    ui->inputTabWidget->setTabEnabled(2, false);
    ui->inputTabWidget->setTabEnabled(3, false);
    ui->outputTabWidget->setTabEnabled(0, true);
    switch (ui->typeBox->currentIndex()) {
        case PlainText:
        case IataBcbp:
        case Uic9183:
            m_sourceDoc->setMode(QStringLiteral("Normal"));
            m_sourceView->show();
            break;
        case Html:
            m_sourceDoc->setMode(QStringLiteral("HTML"));
            m_sourceView->show();
            ui->inputTabWidget->setTabEnabled(1, true);
            ui->inputTabWidget->setTabEnabled(3, true);
            break;
        case Pdf:
        case Image:
            ui->inputTabWidget->setTabEnabled(1, true);
            ui->inputTabWidget->setTabEnabled(2, true);
            m_sourceView->hide();
            break;
        case PkPass:
            m_sourceView->hide();
            break;
        case JsonLd:
            m_sourceDoc->setMode(QStringLiteral("JSON"));
            m_sourceView->show();
            ui->outputTabWidget->setTabEnabled(0, false);
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
    } else if (ui->typeBox->currentIndex() == Uic9183) {
        Uic9183Parser p;
        p.parse(m_sourceDoc->text().toLatin1());
        data = JsonLdDocument::toJson({QVariant::fromValue(p)});
    } else if (ui->typeBox->currentIndex() == PkPass && m_pkpass) {
        auto extractors = m_repo.extractorsForPass(m_pkpass.get());
        ExtractorEngine engine;
        engine.setSenderDate(ui->contextDate->dateTime());
        engine.setPass(m_pkpass.get());
        engine.setExtractors(std::move(extractors));
        data = engine.extract();
    } else if (ui->typeBox->currentIndex() == JsonLd) {
        const auto doc = QJsonDocument::fromJson(m_sourceDoc->text().toUtf8());
        if (doc.isArray())
            data = doc.array();
        else if (doc.isObject())
            data = {doc.object()};
    } else if (ui->typeBox->currentIndex() == Image) {
        auto item = new QStandardItem;
        item->setData(m_image, Qt::DecorationRole);
        m_imageModel->appendRow(item);
    } else {
        ExtractorEngine engine;
        engine.setSenderDate(ui->contextDate->dateTime());

        m_preprocDoc->setReadWrite(true);
        if (ui->typeBox->currentIndex() == PlainText) {
            engine.setText(m_sourceDoc->text());
            m_preprocDoc->setText(m_sourceDoc->text());
        } else if (ui->typeBox->currentIndex() == Html) {
            auto codec = QTextCodec::codecForName(m_sourceDoc->encoding().toUtf8());
            if (!codec) {
                codec = QTextCodec::codecForLocale();
            }

            m_htmlDoc.reset(HtmlDocument::fromData(codec->fromUnicode(m_sourceDoc->text())));
            engine.setHtmlDocument(m_htmlDoc.get());
            if (m_htmlDoc)
                m_preprocDoc->setText(m_htmlDoc->root().recursiveContent());
            else
                m_preprocDoc->clear();
            m_domModel->setDocument(m_htmlDoc.get());
            ui->domView->expandAll();
        } else if (ui->typeBox->currentIndex() == Pdf && m_pdfDoc) {
            engine.setPdfDocument(m_pdfDoc.get());
            m_preprocDoc->setText(m_pdfDoc->text());

            for (int i = 0; i < m_pdfDoc->imageCount(); ++i) {
                auto item = new QStandardItem;
                item->setData(m_pdfDoc->image(i).image(), Qt::DecorationRole);
                m_imageModel->appendRow(item);
            }
        }
        m_preprocDoc->setReadWrite(false);

        KMime::Message msg;
        msg.from()->fromUnicodeString(ui->senderBox->currentText(), "utf-8");
        auto extractors = m_repo.extractorsForMessage(&msg);
        engine.setExtractors(std::move(extractors));
        data = engine.extract();
    }

    m_outputDoc->setReadWrite(true);
    m_outputDoc->setText(QJsonDocument(data).toJson());
    m_outputDoc->setReadWrite(false);

    ExtractorPostprocessor postproc;
    postproc.setContextDate(ui->contextDate->dateTime());
    postproc.process(JsonLdDocument::fromJson(data));

    m_postprocDoc->setReadWrite(true);
    m_postprocDoc->setText(QJsonDocument(JsonLdDocument::toJson(postproc.result())).toJson());
    m_postprocDoc->setReadWrite(false);
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
    } else if (url.toString().endsWith(QLatin1String(".html"))) {
        QFile f(url.toLocalFile());
        f.open(QFile::ReadOnly);
        m_htmlDoc.reset(KItinerary::HtmlDocument::fromData(f.readAll()));
        ui->typeBox->setCurrentIndex(Html);
        m_domModel->setDocument(m_htmlDoc.get());
        ui->domView->expandAll();
        m_sourceDoc->openUrl(url);
        sourceChanged();
    } else if (url.toString().endsWith(QLatin1String(".png")) || url.toString().endsWith(QLatin1String(".jpg"))) {
        m_image.load(url.toLocalFile());
        sourceChanged();
    } else {
        m_sourceDoc->openUrl(url);
    }
}

void MainWindow::imageContextMenu(QPoint pos)
{
    using namespace KItinerary;

    const auto idx = ui->imageView->currentIndex();
    if (!idx.isValid())
        return;

    QMenu menu;
    const auto aztec = menu.addAction(tr("Decode Aztec"));
    const auto aztecBinary = menu.addAction(tr("Decode Aztec (Binary)"));
    const auto pdf417 = menu.addAction(tr("Decode PDF417"));
    const auto qrcode = menu.addAction(tr("Decode QRCode"));
    menu.addSeparator();
    const auto save = menu.addAction(tr("Save..."));
    if (auto action = menu.exec(ui->imageView->viewport()->mapToGlobal(pos))) {
        QString code;
        if (action == aztec) {
            code = BarcodeDecoder::decodeAztec(idx.data(Qt::DecorationRole).value<QImage>());
        } else if (action == aztecBinary) {
            const auto b = BarcodeDecoder::decodeAztecBinary(idx.data(Qt::DecorationRole).value<QImage>());
            code = QString::fromLatin1(b.constData(), b.size());
        } else if (action == pdf417) {
            code = BarcodeDecoder::decodePdf417(idx.data(Qt::DecorationRole).value<QImage>());
        } else if (action == qrcode) {
            code = BarcodeDecoder::decodeQRCode(idx.data(Qt::DecorationRole).value<QImage>());
        } else if (action == save) {
            const auto fileName = QFileDialog::getSaveFileName(this, tr("Save Image"));
            idx.data(Qt::DecorationRole).value<QImage>().save(fileName);
        }
        m_sourceDoc->setText(code);
    }
}
