/*
    SPDX-FileCopyrightText: 2018 Volker Krause <vkrause@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "attributemodel.h"
#include "documentmodel.h"
#include "dommodel.h"
#include "settingsdialog.h"
#include "standarditemmodelhelper.h"

#include <KItinerary/BarcodeDecoder>
#include <KItinerary/BERElement>
#include <KItinerary/CalendarHandler>
#include <KItinerary/ExtractorPostprocessor>
#include <KItinerary/ExtractorRepository>
#include <KItinerary/ExtractorResult>
#include <KItinerary/ExtractorValidator>
#include <KItinerary/HtmlDocument>
#include <KItinerary/IataBcbp>
#include <KItinerary/JsonLdDocument>
#include <KItinerary/MergeUtil>
#include <KItinerary/PdfDocument>
#include <KItinerary/Reservation>
#include <KItinerary/SSBv1Ticket>
#include <KItinerary/SSBv2Ticket>
#include <KItinerary/SSBv3Ticket>
#include <KItinerary/Uic9183Parser>
#include <KItinerary/VdvTicket>
#include <KItinerary/VdvTicketContent>

#include <KPkPass/Pass>

#include <KCalendarCore/Event>
#include <KCalendarCore/ICalFormat>
#include <KCalendarCore/MemoryCalendar>

#include <KMime/Message>

#include <KTextEditor/Document>
#include <KTextEditor/View>
#include <KTextEditor/Editor>

#include <KIO/StoredTransferJob>

#include <KActionCollection>
#include <KLocalizedString>
#include <KStandardAction>

#include <QClipboard>
#include <QDebug>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QMetaEnum>
#include <QMetaObject>
#include <QMimeData>
#include <QSettings>
#include <QStandardItemModel>
#include <QTextCodec>
#include <QToolBar>

#include <cctype>
#include <cstring>

Q_DECLARE_METATYPE(KItinerary::Internal::OwnedPtr<KItinerary::HtmlDocument>)
Q_DECLARE_METATYPE(KItinerary::Internal::OwnedPtr<KItinerary::PdfDocument>)

static QVector<QVector<QVariant>> batchReservations(const QVector<QVariant> &reservations)
{
    using namespace KItinerary;

    QVector<QVector<QVariant>> batches;
    QVector<QVariant> batch;

    for (const auto &res : reservations) {
        if (batch.isEmpty()) {
            batch.push_back(res);
            continue;
        }

        if (JsonLd::canConvert<Reservation>(res) && JsonLd::canConvert<Reservation>(batch.at(0))) {
            const auto trip1 = JsonLd::convert<Reservation>(res).reservationFor();
            const auto trip2 = JsonLd::convert<Reservation>(batch.at(0)).reservationFor();
            if (KItinerary::MergeUtil::isSame(trip1, trip2)) {
                batch.push_back(res);
                continue;
            }
        }

        batches.push_back(batch);
        batch.clear();
        batch.push_back(res);
    }

    if (!batch.isEmpty()) {
        batches.push_back(batch);
    }
    return batches;
}

MainWindow::MainWindow(QWidget* parent)
    : KXmlGuiWindow(parent)
    , ui(new Ui::MainWindow)
    , m_extractorDocModel(new DocumentModel(this))
    , m_imageModel(new QStandardItemModel(this))
    , m_domModel(new DOMModel(this))
    , m_attrModel(new AttributeModel(this))
    , m_iataBcbpModel(new QStandardItemModel(this))
    , m_eraSsbModel(new QStandardItemModel(this))
    , m_vdvModel(new QStandardItemModel(this))
{
    ui->setupUi(this);
    ui->contextDate->setDateTime(QDateTime(QDate::currentDate(), QTime()));
    setCentralWidget(ui->mainSplitter);

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

    ui->documentTreeView->setModel(m_extractorDocModel);
    ui->documentTreeView->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    connect(ui->documentTreeView->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this](const QItemSelection &selection) {
        if (selection.empty()) {
            return;
        }
        auto idx = selection.at(0).topLeft();
        idx = idx.sibling(idx.row(), 0);
        setCurrentDocumentNode(idx.data(Qt::UserRole).value<KItinerary::ExtractorDocumentNode>());
    });

    m_imageModel->setHorizontalHeaderLabels({i18n("Image")});
    ui->imageView->setModel(m_imageModel);
    connect(ui->imageView, &QWidget::customContextMenuRequested, this, &MainWindow::imageContextMenu);

    auto domFilterModel = new DOMFilterModel(this);
    domFilterModel->setRecursiveFilteringEnabled(true);
    domFilterModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    domFilterModel->setSourceModel(m_domModel);
    connect(domFilterModel, &QSortFilterProxyModel::layoutChanged, ui->domView, &QTreeView::expandAll);
    connect(domFilterModel, &QSortFilterProxyModel::rowsRemoved, ui->domView, &QTreeView::expandAll);
    connect(domFilterModel, &QSortFilterProxyModel::rowsInserted, ui->domView, &QTreeView::expandAll);
    ui->domView->setModel(domFilterModel);
    ui->domView->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    connect(ui->domSearchLine, &QLineEdit::textChanged, domFilterModel, &QSortFilterProxyModel::setFilterFixedString);
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
        if (!m_domModel->document()) {
            return;
        }
        const auto res = m_domModel->document()->eval(ui->xpathEdit->text());
        if (!res.canConvert<QVariantList>()) { // TODO show this properly in the UI somehow
            qDebug() << "XPath result:" << res;
        }
        m_domModel->setHighlightNodeSet(res.value<QVariantList>());
        ui->domView->viewport()->update(); // dirty, but easier than triggering a proper full model update
    });

    m_iataBcbpModel->setHorizontalHeaderLabels({i18n("Field"), i18n("Value")});
    ui->iataBcbpView->setModel(m_iataBcbpModel);
    ui->iataBcbpView->header()->setSectionResizeMode(QHeaderView::ResizeToContents);

    m_eraSsbModel->setHorizontalHeaderLabels({i18n("Field"), i18n("Value")});
    ui->eraSsbView->setModel(m_eraSsbModel);
    ui->eraSsbView->header()->setSectionResizeMode(QHeaderView::ResizeToContents);

    m_vdvModel->setHorizontalHeaderLabels({i18n("Field"), i18n("Value")});
    ui->vdvView->setModel(m_vdvModel);
    ui->vdvView->header()->setSectionResizeMode(QHeaderView::ResizeToContents);

    m_nodeResultDoc = editor->createDocument(nullptr);
    m_nodeResultDoc->setMode(QStringLiteral("JSON"));
    view = m_nodeResultDoc->createView(nullptr);
    layout = new QHBoxLayout(ui->nodeResultTab);
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

    m_validatedDoc = editor->createDocument(nullptr);
    m_validatedDoc->setMode(QStringLiteral("JSON"));
    view = m_validatedDoc->createView(nullptr);
    ui->validatedTab->layout()->addWidget(view);
    connect(ui->acceptCompleteOnly, &QCheckBox::toggled, this, &MainWindow::sourceChanged);

    m_icalDoc = editor->createDocument(nullptr);
    m_icalDoc->setMode(QStringLiteral("vCard, vCalendar, iCalendar"));
    view = m_icalDoc->createView(nullptr);
    layout = new QHBoxLayout(ui->icalTab);
    layout->addWidget(view);

    connect(ui->consoleWidget, &ConsoleOutputWidget::navigateToSource, ui->extractorWidget, &ExtractorEditorWidget::navigateToSource);
    connect(ui->consoleWidget, &ConsoleOutputWidget::navigateToSource, this, [this]() {
        ui->inputTabWidget->setCurrentIndex(ExtractorEditorTab);
    });

    setCurrentDocumentNode({});

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
    connect(ui->actionInputFromClipboard, &QAction::triggered, this, &MainWindow::loadFromClipboard);
    connect(ui->actionInputClear, &QAction::triggered, this, [this]() {
        ui->fileRequester->clear();
        m_sourceDoc->clear();
        m_sourceView->show();
        sourceChanged();
    });
    connect(ui->actionSeparateProcess, &QAction::toggled, this, [this](bool checked) {
        clearEngine();
        m_engine.setUseSeparateProcess(checked);
        sourceChanged();
    });
    connect(ui->actionFullPageRasterImages, &QAction::toggled, this, [this](bool checked) {
        clearEngine();
        if (checked) {
            m_engine.setHints(m_engine.hints() | KItinerary::ExtractorEngine::ExtractFullPageRasterImages);
        } else {
            m_engine.setHints(m_engine.hints() & ~KItinerary::ExtractorEngine::ExtractFullPageRasterImages);
        }
        sourceChanged();
    });
    connect(ui->actionSettingsConfigure, &QAction::triggered, this, [this]() {
        SettingsDialog dlg(this);
        if (dlg.exec() == QDialog::Accepted) {
            ui->actionExtractorReloadRepository->trigger();
        }
    });
    actionCollection()->addAction(QStringLiteral("extractor_run"), ui->actionExtractorRun);
    actionCollection()->addAction(QStringLiteral("extractor_reload_repository"), ui->actionExtractorReloadRepository);
    actionCollection()->addAction(QStringLiteral("input_from_clipboard"), ui->actionInputFromClipboard);
    actionCollection()->addAction(QStringLiteral("input_clear"), ui->actionInputClear);
    actionCollection()->addAction(QStringLiteral("file_quit"), KStandardAction::quit(QApplication::instance(), &QApplication::closeAllWindows, this));
    actionCollection()->addAction(QStringLiteral("options_configure"), ui->actionSettingsConfigure);
    actionCollection()->addAction(QStringLiteral("settings_separate_process"), ui->actionSeparateProcess);
    actionCollection()->addAction(QStringLiteral("settings_full_page_raster_images"), ui->actionFullPageRasterImages);
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

    clearEngine();
}

void MainWindow::openFile(const QString &file)
{
    ui->fileRequester->setText(file);
}

void MainWindow::clearEngine()
{
    // ensure we hold no references to document nodes anymore
    ui->documentTreeView->clearSelection();
    m_currentNode = {};
    StandardItemModelHelper::clearContent(m_extractorDocModel);
    m_engine.clear();
}

void MainWindow::sourceChanged()
{
    clearEngine();

    StandardItemModelHelper::clearContent(m_imageModel);
    ui->uic9183Widget->clear();
    ui->consoleWidget->clear();
    using namespace KItinerary;

    if (m_sourceView->isVisible()) {
        auto codec = QTextCodec::codecForName(m_sourceDoc->encoding().toUtf8());
        if (!codec) {
            codec = QTextCodec::codecForLocale();
        }
        m_data = codec->fromUnicode(m_sourceDoc->text());
    }

    m_contextMsg = std::make_unique<KMime::Message>();
    m_contextMsg->from()->fromUnicodeString(ui->senderBox->currentText(), "utf-8");
    m_contextMsg->date()->setDateTime(ui->contextDate->dateTime());
    m_engine.setContext(QVariant::fromValue<KMime::Content*>(m_contextMsg.get()), u"message/rfc822");

    m_engine.setData(m_data, ui->fileRequester->url().toString());
    const auto data = m_engine.extract();
    ui->extractorWidget->showExtractor(m_engine.usedCustomExtractor());

    m_extractorDocModel->setRootNode(m_engine.rootDocumentNode());
    ui->documentTreeView->expandAll();
    setCurrentDocumentNode(m_engine.rootDocumentNode());

    m_outputDoc->setReadWrite(true);
    m_outputDoc->setText(QString::fromUtf8(QJsonDocument(data).toJson()));
    m_outputDoc->setReadWrite(false);

    ExtractorPostprocessor postproc;
    postproc.setContextDate(ui->contextDate->dateTime());
    postproc.setValidationEnabled(false);
    postproc.process(JsonLdDocument::fromJson(data));
    auto result = postproc.result();

    m_postprocDoc->setReadWrite(true);
    m_postprocDoc->setText(QString::fromUtf8(QJsonDocument(JsonLdDocument::toJson(result)).toJson()));
    m_postprocDoc->setReadWrite(false);

    ExtractorValidator validator;
    validator.setAcceptOnlyCompleteElements(ui->acceptCompleteOnly->isChecked());
    result.erase(std::remove_if(result.begin(), result.end(), [&validator](const auto &elem) {
        return !validator.isValidElement(elem);
    }), result.end());
    m_validatedDoc->setReadWrite(true);
    m_validatedDoc->setText(QString::fromUtf8(QJsonDocument(JsonLdDocument::toJson(result)).toJson()));
    m_validatedDoc->setReadWrite(false);

    const auto batches = batchReservations(result);
    KCalendarCore::Calendar::Ptr cal(new KCalendarCore::MemoryCalendar(QTimeZone::systemTimeZone()));
    for (const auto &batch : batches) {
        KCalendarCore::Event::Ptr event(new KCalendarCore::Event);
        CalendarHandler::fillEvent(batch, event);
        cal->addEvent(event);
    }
    KCalendarCore::ICalFormat format;
    m_icalDoc->setText(format.toString(cal));
}

void MainWindow::urlChanged()
{
    const auto url = ui->fileRequester->url();
    if (!url.isValid()) {
        return;
    }

    auto job = KIO::storedGet(url);
    connect(job, &KJob::finished, this, [this, job, url]() {
        if (job->error() != KJob::NoError) {
            qWarning() << job->errorString();
            return;
        }
        m_data = job->data();
        const auto isText = std::none_of(m_data.begin(), m_data.end(), [](unsigned char c) { return std::iscntrl(c) && !std::isspace(c); });
        const auto textExt =
            url.fileName().endsWith(QLatin1String(".eml")) ||
            url.fileName().endsWith(QLatin1String(".html")) ||
            url.fileName().endsWith(QLatin1String(".mbox")) ||
            url.fileName().endsWith(QLatin1String(".txt"));
        if (isText || textExt) {
            if (url.scheme() == QLatin1String("https") || url.scheme() == QLatin1String("http")) {
                m_sourceDoc->setText(QString::fromUtf8(m_data));
            } else {
                m_sourceDoc->openUrl(url);
            }
            m_sourceView->show();
        } else {
            m_sourceView->hide();
            sourceChanged();
        }
    });
}

void MainWindow::loadFromClipboard()
{
    ui->fileRequester->clear();

    const auto md = QGuiApplication::clipboard()->mimeData();
    if (md->hasText()) {
        m_sourceDoc->setText(md->text());
        m_sourceView->show();
    } else if (md->hasFormat(QLatin1String("application/octet-stream"))) {
        m_data = md->data(QLatin1String("application/octet-stream"));
        m_sourceView->hide();
    }
    sourceChanged();
}

void MainWindow::imageContextMenu(QPoint pos)
{
    using namespace KItinerary;

    const auto idx = ui->imageView->currentIndex();
    if (!idx.isValid())
        return;

    QMenu menu;
    const auto barcode = menu.addAction(i18n("Decode && Copy Barcode"));
    const auto barcodeBinary = menu.addAction(i18n("Decode && Copy Barcode (Binary)"));
    menu.addSeparator();
    const auto save = menu.addAction(i18n("Save Image..."));
    const auto bcSave = menu.addAction(i18n("Save Barcode Content..."));
    if (auto action = menu.exec(ui->imageView->viewport()->mapToGlobal(pos))) {
        if (action == barcode) {
            BarcodeDecoder decoder;
            const auto code = decoder.decodeString(idx.data(Qt::DecorationRole).value<QImage>(), BarcodeDecoder::Any | BarcodeDecoder::IgnoreAspectRatio);
            QGuiApplication::clipboard()->setText(code);
        } else if (action == barcodeBinary) {
            BarcodeDecoder decoder;
            const auto b = decoder.decodeBinary(idx.data(Qt::DecorationRole).value<QImage>(), BarcodeDecoder::Any | BarcodeDecoder::IgnoreAspectRatio);
            auto md = new QMimeData;
            md->setData(QStringLiteral("application/octet-stream"), b);
            QGuiApplication::clipboard()->setMimeData(md);
        } else if (action == save) {
            const auto fileName = QFileDialog::getSaveFileName(this, i18n("Save Image"));
            idx.data(Qt::DecorationRole).value<QImage>().save(fileName);
        } else if (action == bcSave) {
            const auto fileName = QFileDialog::getSaveFileName(this, i18n("Save Barcode Content"));
            if (!fileName.isEmpty()) {
                BarcodeDecoder decoder;
                const auto b = decoder.decodeBinary(idx.data(Qt::DecorationRole).value<QImage>());
                QFile f(fileName);
                if (!f.open(QFile::WriteOnly)) {
                    qWarning() << "Failed to open file:" << f.errorString() << fileName;
                } else {
                    f.write(b);
                }
            }
        }
    }
}

void MainWindow::setCurrentDocumentNode(const KItinerary::ExtractorDocumentNode &node)
{
    for (auto i : { TextTab, ImageTab, DomTab, Uic9183Tab, IataBcbpTab, EraSsbTab, VdvTab }) {
        ui->inputTabWidget->setTabEnabled(i, false);
    }

    StandardItemModelHelper::clearContent(m_imageModel);
    m_domModel->setDocument(nullptr);

    using namespace KItinerary;
    m_currentNode = node;
    m_nodeResultDoc->setText(QString::fromUtf8(QJsonDocument(node.result().jsonLdResult()).toJson()));

    if (node.mimeType() == QLatin1String("application/pdf")) {
        const auto pdf = node.content<PdfDocument*>();
        if (!pdf) {
            return;
        }
        m_preprocDoc->setText(pdf->text());

        for (int i = 0; i < pdf->pageCount(); ++i) {
            auto pageItem = new QStandardItem;
            pageItem->setText(i18n("Page %1", i + 1));
            const auto page = pdf->page(i);
            for (int j = 0; j < page.imageCount(); ++j) {
                auto imgItem = new QStandardItem;
                auto pdfImg = page.image(j);
                auto imgData = pdfImg.image();
                bool skippedByExtractor = false;
                if (imgData.isNull()) {
                    skippedByExtractor = true;
                    pdfImg.setLoadingHints(PdfImage::NoHint);
                    imgData = pdfImg.image();
                }
                imgItem->setData(imgData, Qt::DecorationRole);
                imgItem->setToolTip(i18n("Size: %1 x %2\nSource: %3 x %4\nSkipped: %5", pdfImg.width(), pdfImg.height(), pdfImg.sourceWidth(), pdfImg.sourceHeight(), skippedByExtractor));
                pageItem->appendRow(imgItem);
            }
            m_imageModel->appendRow(pageItem);
        }
        ui->imageView->expandAll();

        ui->inputTabWidget->setTabEnabled(TextTab, true);
        ui->inputTabWidget->setTabEnabled(ImageTab, true);
    }
    else if (node.mimeType() == QLatin1String("internal/qimage")) {
        auto item = new QStandardItem;
        item->setData(node.content<QImage>(), Qt::DecorationRole);
        m_imageModel->appendRow(item);
        ui->inputTabWidget->setTabEnabled(ImageTab, true);
    }
    else if (node.mimeType() == QLatin1String("internal/uic9183")) {
        const auto uic9183 = node.content<Uic9183Parser>();
        ui->uic9183Widget->setContent(uic9183);
        ui->inputTabWidget->setTabEnabled(Uic9183Tab, true);
    }
    else if (node.mimeType() == QLatin1String("text/html")) {
        const auto html = node.content<HtmlDocument*>();
        m_domModel->setDocument(html);
        m_preprocDoc->setText(html->root().recursiveContent());
        ui->domView->expandAll();
        ui->inputTabWidget->setTabEnabled(TextTab, true);
        ui->inputTabWidget->setTabEnabled(DomTab, true);
    }
    else if (node.mimeType() == QLatin1String("text/plain")) {
        m_preprocDoc->setText(node.content().value<QString>());
        ui->inputTabWidget->setTabEnabled(TextTab, true);
    }
    else if (node.mimeType() == QLatin1String("application/ld+json")) {
        m_preprocDoc->setText(QString::fromUtf8(QJsonDocument(node.content().value<QJsonArray>()).toJson()));
        ui->inputTabWidget->setTabEnabled(TextTab, true);
    }
    else if (node.mimeType() == QLatin1String("internal/iata-bcbp")) {
        const auto bcbp = node.content<IataBcbp>();
        m_preprocDoc->setText(bcbp.rawData());
        ui->inputTabWidget->setTabEnabled(TextTab, true);

        StandardItemModelHelper::clearContent(m_iataBcbpModel);
        const auto ums = bcbp.uniqueMandatorySection();
        StandardItemModelHelper::fillFromGadget(ums, m_iataBcbpModel->invisibleRootItem());
        const auto ucs = bcbp.uniqueConditionalSection();
        StandardItemModelHelper::fillFromGadget(ucs, m_iataBcbpModel->invisibleRootItem());
        const auto issueDate = ucs.dateOfIssue(node.contextDateTime());
        StandardItemModelHelper::addEntry(i18n("Date of issue"), issueDate.toString(Qt::ISODate), m_iataBcbpModel->invisibleRootItem());
        for (auto i = 0; i < ums.numberOfLegs(); ++i) {
            auto legItem = StandardItemModelHelper::addEntry(i18n("Leg %1", i + 1), {}, m_iataBcbpModel->invisibleRootItem());
            const auto rms = bcbp.repeatedMandatorySection(i);
            StandardItemModelHelper::fillFromGadget(rms, legItem);
            const auto rcs = bcbp.repeatedConditionalSection(i);
            StandardItemModelHelper::fillFromGadget(rcs, legItem);
            StandardItemModelHelper::addEntry(i18n("Airline use section"), bcbp.airlineUseSection(i), legItem);
            StandardItemModelHelper::addEntry(i18n("Date of flight"), rms.dateOfFlight(issueDate.isValid() ? QDateTime(issueDate, {}) : node.contextDateTime()).toString(Qt::ISODate), legItem);
        }

        if (bcbp.hasSecuritySection()) {
            auto secItem = StandardItemModelHelper::addEntry(i18n("Security"), {}, m_iataBcbpModel->invisibleRootItem());
            const auto sec = bcbp.securitySection();
            StandardItemModelHelper::fillFromGadget(sec, secItem);
        }

        ui->iataBcbpView->expandAll();
        ui->inputTabWidget->setTabEnabled(IataBcbpTab, true);
    }
    else if (node.mimeType() == QLatin1String("internal/era-ssb")) {
        StandardItemModelHelper::clearContent(m_eraSsbModel);
        if (node.isA<SSBv1Ticket>()) {
            const auto ssb = node.content<SSBv1Ticket>();
            StandardItemModelHelper::fillFromGadget(ssb, m_eraSsbModel->invisibleRootItem());
            StandardItemModelHelper::addEntry(i18n("First day of validity"), ssb.firstDayOfValidity(node.contextDateTime()).toString(Qt::ISODate), m_eraSsbModel->invisibleRootItem());
            StandardItemModelHelper::addEntry(i18n("Departure time"), ssb.departureTime(node.contextDateTime()).toString(Qt::ISODate), m_eraSsbModel->invisibleRootItem());
        } else if (node.isA<SSBv2Ticket>()) {
            const auto ssb = node.content<SSBv2Ticket>();
            StandardItemModelHelper::fillFromGadget(ssb, m_eraSsbModel->invisibleRootItem());
            StandardItemModelHelper::addEntry(i18n("First day of validity"), ssb.firstDayOfValidity(node.contextDateTime()).toString(Qt::ISODate), m_eraSsbModel->invisibleRootItem());
            StandardItemModelHelper::addEntry(i18n("Last day of validity"), ssb.lastDayOfValidity(node.contextDateTime()).toString(Qt::ISODate), m_eraSsbModel->invisibleRootItem());
        } else if (node.isA<SSBv3Ticket>()) {
            const auto ssb = node.content<SSBv3Ticket>();
            const auto typePrefix = QByteArray("type" + QByteArray::number(ssb.ticketTypeCode()));
            for (auto i = 0; i < SSBv3Ticket::staticMetaObject.propertyCount(); ++i) {
                const auto prop = SSBv3Ticket::staticMetaObject.property(i);
                if (!prop.isStored() || (std::strncmp(prop.name(), "type", 4) == 0 && std::strncmp(prop.name(), typePrefix.constData(), 5) != 0)) {
                    continue;
                }
                const auto value = prop.readOnGadget(&ssb);
                StandardItemModelHelper::addEntry(QString::fromUtf8(prop.name()), value.toString(), m_eraSsbModel->invisibleRootItem());
            }
            StandardItemModelHelper::addEntry(i18n("Issuing day"), ssb.issueDate(node.contextDateTime()).toString(Qt::ISODate), m_eraSsbModel->invisibleRootItem());
            if (ssb.ticketTypeCode() == SSBv3Ticket::IRT_RES_BOA) {
                StandardItemModelHelper::addEntry(i18n("Departure day"), ssb.type1DepartureDay(node.contextDateTime()).toString(Qt::ISODate), m_eraSsbModel->invisibleRootItem());
            }
        } else {
            StandardItemModelHelper::fillFromGadget(node.content(), m_eraSsbModel->invisibleRootItem());
        }

        ui->eraSsbView->expandAll();
        ui->inputTabWidget->setTabEnabled(EraSsbTab, true);
    }
    else if (node.mimeType() == QLatin1String("internal/vdv")) {
        StandardItemModelHelper::clearContent(m_vdvModel);
        const auto vdv = node.content<VdvTicket>();
        auto item = StandardItemModelHelper::addEntry(i18n("Header"), {}, m_vdvModel->invisibleRootItem());
        StandardItemModelHelper::fillFromGadget(vdv.header(), item);

        item = StandardItemModelHelper::addEntry(i18n("Product data"), {}, m_vdvModel->invisibleRootItem());
        for (auto block = vdv.productData().first(); block.isValid(); block = block.next()) {
            auto blockItem = StandardItemModelHelper::addEntry(i18n("Block 0x%1 (%2 bytes)", QString::number(block.type(), 16), block.size()), {}, item);
            switch (block.type()) {
                case VdvTicketBasicData::Tag:
                    StandardItemModelHelper::fillFromGadget(block.contentAt<VdvTicketBasicData>(), blockItem);
                    break;
            case VdvTicketTravelerData::Tag:
            {
                const auto traveler = block.contentAt<VdvTicketTravelerData>();
                StandardItemModelHelper::fillFromGadget(traveler, blockItem);
                StandardItemModelHelper::addEntry(i18n("Name"), QString::fromUtf8(traveler->name(), traveler->nameSize(block.contentSize())), blockItem);
                break;
            }
            case VdvTicketValidityAreaData::Tag:
            {
                const auto area = block.contentAt<VdvTicketValidityAreaData>();

                switch (area->type) {
                    case VdvTicketValidityAreaDataType31::Type:
                    {
                        const auto area31 = static_cast<const VdvTicketValidityAreaDataType31*>(area);
                        StandardItemModelHelper::fillFromGadget(area31, blockItem);
                        StandardItemModelHelper::addEntry(i18n("Payload"), StandardItemModelHelper::dataToHex(block.contentData(), block.contentSize(), sizeof(VdvTicketValidityAreaDataType31)), blockItem);
                        break;
                    }
                    default:
                        StandardItemModelHelper::fillFromGadget(area, blockItem);
                        StandardItemModelHelper::addEntry(i18n("Payload"), StandardItemModelHelper::dataToHex(block.contentData(), block.contentSize(), sizeof(VdvTicketValidityAreaData)), blockItem);
                        break;
                }
                break;
            }
            default:
                StandardItemModelHelper::addEntry(i18n("Data"), StandardItemModelHelper::dataToHex(block.contentData(), block.contentSize()), blockItem);
            }
        }

        item = StandardItemModelHelper::addEntry(i18n("Transaction data"), {}, m_vdvModel->invisibleRootItem());
        StandardItemModelHelper::fillFromGadget(vdv.commonTransactionData(), item);
        item = StandardItemModelHelper::addEntry(i18n("Product-specific transaction data (%1 bytes)", vdv.productSpecificTransactionData().contentSize()), {}, m_vdvModel->invisibleRootItem());
        for (auto block = vdv.productSpecificTransactionData().first(); block.isValid(); block = block.next()) {
            auto blockItem = StandardItemModelHelper::addEntry(i18n("Tag 0x%1 (%2 bytes)", QString::number(block.type(), 16), block.size()), {}, item);
            switch (block.type()) {
                default:
                    StandardItemModelHelper::addEntry(i18n("Data"), StandardItemModelHelper::dataToHex(block.contentData(), block.contentSize()), blockItem);
            }
        }

        item = StandardItemModelHelper::addEntry(i18n("Issue data"), {}, m_vdvModel->invisibleRootItem());
        StandardItemModelHelper::fillFromGadget(vdv.issueData(), item);
        item = StandardItemModelHelper::addEntry(i18n("Trailer"), {}, m_vdvModel->invisibleRootItem());
        StandardItemModelHelper::addEntry(i18n("identifier"), QString::fromUtf8(vdv.trailer()->identifier, 3), item);
        StandardItemModelHelper::fillFromGadget(vdv.trailer(), item);

        ui->vdvView->expandAll();
        ui->inputTabWidget->setTabEnabled(VdvTab, true);
    }
}
