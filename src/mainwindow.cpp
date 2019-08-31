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
#include "settingsdialog.h"

#include <KItinerary/BarcodeDecoder>
#include <KItinerary/CalendarHandler>
#include <KItinerary/ExtractorEngine>
#include <KItinerary/ExtractorPostprocessor>
#include <KItinerary/ExtractorRepository>
#include <KItinerary/HtmlDocument>
#include <KItinerary/IataBcbpParser>
#include <KItinerary/JsonLdDocument>
#include <KItinerary/PdfDocument>

#include <KPkPass/Pass>

#include <KCalendarCore/Event>
#include <KCalendarCore/ICalFormat>
#include <KCalendarCore/MemoryCalendar>

#include <KMime/Message>

#include <KTextEditor/Document>
#include <KTextEditor/View>
#include <KTextEditor/Editor>

#include <KActionCollection>
#include <KLocalizedString>
#include <KStandardAction>

#include <QClipboard>
#include <QDebug>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QMimeData>
#include <QSettings>
#include <QStandardItemModel>
#include <QTextCodec>
#include <QToolBar>

MainWindow::MainWindow(QWidget* parent)
    : KXmlGuiWindow(parent)
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
    connect(ui->extractorWidget, &ExtractorEditorWidget::extractorChanged, this, &MainWindow::sourceChanged);

    auto editor = KTextEditor::Editor::instance();

    m_sourceDoc = editor->createDocument(nullptr);
    connect(m_sourceDoc, &KTextEditor::Document::textChanged, this, &MainWindow::sourceChanged);
    m_sourceView = m_sourceDoc->createView(nullptr);
    ui->sourceTab->layout()->addWidget(m_sourceView);

    m_preprocDoc = editor->createDocument(nullptr);
    auto view = m_preprocDoc->createView(nullptr);
    auto layout = new QHBoxLayout(ui->preprocTab);
    layout->addWidget(view);

    m_imageModel->setHorizontalHeaderLabels({i18n("Image")});
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
    connect(ui->xpathEdit, &QLineEdit::editingFinished, this, [this]() {
        if (!m_htmlDoc) {
            return;
        }
        const auto res = m_htmlDoc->eval(ui->xpathEdit->text());
        if (!res.canConvert<QVariantList>()) { // TODO show this properly in the UI somehow
            qDebug() << "XPath result:" << res;
        }
        m_domModel->setHighlightNodeSet(res.value<QVariantList>());
        ui->domView->viewport()->update(); // dirty, but easier than triggering a proper full model update
    });

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

    m_icalDoc = editor->createDocument(nullptr);
    m_icalDoc->setMode(QStringLiteral("vCard, vCalendar, iCalendar"));
    view = m_icalDoc->createView(nullptr);
    layout = new QHBoxLayout(ui->icalTab);
    layout->addWidget(view);

    connect(ui->consoleWidget, &ConsoleOutputWidget::navigateToSource, ui->extractorWidget, &ExtractorEditorWidget::navigateToSource);
    connect(ui->consoleWidget, &ConsoleOutputWidget::navigateToSource, this, [this]() {
        ui->inputTabWidget->setCurrentIndex(ExtractorEditorTab);
    });

    typeChanged();

    QSettings settings;
    settings.beginGroup(QLatin1String("SenderHistory"));
    ui->senderBox->addItems(settings.value(QLatin1String("History")).toStringList());
    ui->senderBox->setCurrentText(QString());

    connect(ui->actionExtractorRun, &QAction::triggered, this, &MainWindow::sourceChanged);
    connect(ui->actionExtractorReloadRepository, &QAction::triggered, this, [this]() {
        KItinerary::ExtractorRepository repo;
        repo.reload();
        ui->extractorWidget->reloadExtractors();
    });
    connect(ui->actionSettingsConfigure, &QAction::triggered, this, [this]() {
        SettingsDialog dlg(this);
        if (dlg.exec() == QDialog::Accepted) {
            ui->actionExtractorReloadRepository->trigger();
        }
    });
    actionCollection()->addAction(QStringLiteral("extractor_run"), ui->actionExtractorRun);
    actionCollection()->addAction(QStringLiteral("extractor_reload_repository"), ui->actionExtractorReloadRepository);
    actionCollection()->addAction(QStringLiteral("file_quit"), KStandardAction::quit(QApplication::instance(), &QApplication::closeAllWindows, this));
    actionCollection()->addAction(QStringLiteral("options_configure"), ui->actionSettingsConfigure);
    ui->extractorWidget->registerActions(actionCollection());

    setupGUI(Default, QStringLiteral("ui.rc"));
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
}

void MainWindow::openFile(const QString &file)
{
    ui->fileRequester->setText(file);
}

void MainWindow::typeChanged()
{
    ui->inputTabWidget->setTabEnabled(TextTab, false);
    ui->inputTabWidget->setTabEnabled(ImageTab, false);
    ui->inputTabWidget->setTabEnabled(DomTab, false);
    ui->inputTabWidget->setTabEnabled(Uic9183Tab, false);
    ui->outputTabWidget->setTabEnabled(ExtractorOutputTab, true);
    switch (ui->typeBox->currentIndex()) {
        case PlainText:
        case IataBcbp:
            m_sourceDoc->setMode(QStringLiteral("Normal"));
            m_sourceView->show();
            break;
        case Uic9183:
            m_sourceDoc->setMode(QStringLiteral("Normal"));
            m_sourceView->show();
            ui->inputTabWidget->setTabEnabled(Uic9183Tab, true);
            break;
        case Html:
            m_sourceDoc->setMode(QStringLiteral("HTML"));
            m_sourceView->show();
            ui->inputTabWidget->setTabEnabled(TextTab, true);
            ui->inputTabWidget->setTabEnabled(DomTab, true);
            break;
        case Pdf:
        case Image:
            ui->inputTabWidget->setTabEnabled(TextTab, true);
            ui->inputTabWidget->setTabEnabled(ImageTab, true);
            m_sourceView->hide();
            break;
        case PkPass:
            m_sourceView->hide();
            break;
        case JsonLd:
            m_sourceDoc->setMode(QStringLiteral("JSON"));
            m_sourceView->show();
            ui->outputTabWidget->setTabEnabled(ExtractorOutputTab, false);
            break;
        case ICal:
            m_sourceDoc->setMode(QStringLiteral("vCard, vCalendar, iCalendar"));
            m_sourceView->show();
            break;
        case Mime:
            m_sourceDoc->setMode(QStringLiteral("Email"));
            m_sourceView->show();
            break;
    }
}

void MainWindow::sourceChanged()
{
    m_imageModel->removeRows(0, m_imageModel->rowCount());
    ui->uic9183Widget->clear();
    ui->consoleWidget->clear();
    using namespace KItinerary;

    QJsonArray data;
    if (ui->typeBox->currentIndex() == IataBcbp) {
        const auto bp = IataBcbpParser::parse(m_sourceDoc->text(), ui->contextDate->date());
        data = JsonLdDocument::toJson({bp});
    } else if (ui->typeBox->currentIndex() == Uic9183) {
        m_ticketParser.setContextDate(ui->contextDate->dateTime());
        m_ticketParser.parse(m_sourceDoc->text().toLatin1());
        data = {JsonLdDocument::toJson(QVariant::fromValue(m_ticketParser))};
        ui->uic9183Widget->setContent(m_ticketParser);
    } else if (ui->typeBox->currentIndex() == PkPass && m_pkpass) {
        ExtractorEngine engine;
        engine.setContextDate(ui->contextDate->dateTime());
        engine.setPass(m_pkpass.get());
        data = engine.extract();
        ui->extractorWidget->showExtractor(engine.usedCustomExtractor());
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

            for (int i = 0; i < m_pdfDoc->pageCount(); ++i) {
                auto pageItem = new QStandardItem;
                pageItem->setText(i18n("Page %1", i + 1));
                const auto page = m_pdfDoc->page(i);
                for (int j = 0; j < page.imageCount(); ++j) {
                    auto imgItem = new QStandardItem;
                    imgItem->setData(page.image(j).image(), Qt::DecorationRole);
                    pageItem->appendRow(imgItem);
                }
                m_imageModel->appendRow(pageItem);
            }
            ui->imageView->expandAll();
        } else if (ui->typeBox->currentIndex() == ICal) {
            m_calendar.reset(new KCalendarCore::MemoryCalendar(QTimeZone::systemTimeZone()));
            KCalendarCore::ICalFormat format;
            format.fromString(m_calendar, m_sourceDoc->text());
            m_calendar->setProductId(format.loadedProductId());
            engine.setCalendar(m_calendar);
        } else if (ui->typeBox->currentIndex() == Mime) {
            m_mimeMessage.reset(new KMime::Message);
            m_mimeMessage->setContent(m_sourceDoc->text().toUtf8());
            m_mimeMessage->parse();
            engine.setContent(m_mimeMessage.get());
        }
        m_preprocDoc->setReadWrite(false);

        KMime::Message msg;
        if (ui->typeBox->currentIndex() != Mime) {
            msg.from()->fromUnicodeString(ui->senderBox->currentText(), "utf-8");
            msg.date()->setDateTime(ui->contextDate->dateTime());
            engine.setContext(&msg);
        }

        data = engine.extract();
        ui->extractorWidget->showExtractor(engine.usedCustomExtractor());
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

    KCalendarCore::Calendar::Ptr cal(new KCalendarCore::MemoryCalendar(QTimeZone::systemTimeZone()));
    for (const auto &res : postproc.result()) {
        KCalendarCore::Event::Ptr event(new KCalendarCore::Event);
        CalendarHandler::fillEvent({res}, event); // TODO this assumes multi-traveller batching to have already happened!
        cal->addEvent(event);
    }
    KCalendarCore::ICalFormat format;
    m_icalDoc->setText(format.toString(cal));
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
    } else if (url.toString().endsWith(QLatin1String(".png")) || url.toString().endsWith(QLatin1String(".jpg"))) {
        m_image.load(url.toLocalFile());
        sourceChanged();
    } else if (url.toString().endsWith(QLatin1String(".ics"))) {
        ui->typeBox->setCurrentIndex(ICal);
        m_sourceDoc->openUrl(url);
    } else if (url.toString().endsWith(QLatin1String(".eml")) || url.toString().endsWith(QLatin1String(".mbox"))) {
        ui->typeBox->setCurrentIndex(Mime);
        m_sourceDoc->openUrl(url);
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
    const auto barcode = menu.addAction(i18n("Decode Barcode"));
    const auto barcodeBinary = menu.addAction(i18n("Decode Barcode (Binary)"));
    menu.addSeparator();
    const auto save = menu.addAction(i18n("Save..."));
    if (auto action = menu.exec(ui->imageView->viewport()->mapToGlobal(pos))) {
        QString code;
        if (action == barcode) {
            BarcodeDecoder decoder;
            code = decoder.decodeString(idx.data(Qt::DecorationRole).value<QImage>());
        } else if (action == barcodeBinary) {
            BarcodeDecoder decoder;
            const auto b = decoder.decodeBinary(idx.data(Qt::DecorationRole).value<QImage>());
            code = QString::fromLatin1(b.constData(), b.size());
        } else if (action == save) {
            const auto fileName = QFileDialog::getSaveFileName(this, i18n("Save Image"));
            idx.data(Qt::DecorationRole).value<QImage>().save(fileName);
        }
        m_sourceDoc->setText(code);

        if (IataBcbpParser::maybeIataBcbp(code)) {
            ui->typeBox->setCurrentIndex(IataBcbp);
        } else if (Uic9183Parser::maybeUic9183(code.toLatin1())) {
            ui->typeBox->setCurrentIndex(Uic9183);
        }
    }
}
