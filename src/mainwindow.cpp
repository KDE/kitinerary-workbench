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

#include <KItinerary/BarcodeDecoder>
#include <KItinerary/CalendarHandler>
#include <KItinerary/ExtractorPostprocessor>
#include <KItinerary/ExtractorRepository>
#include <KItinerary/ExtractorValidator>
#include <KItinerary/HtmlDocument>
#include <KItinerary/JsonLdDocument>
#include <KItinerary/MergeUtil>
#include <KItinerary/PdfDocument>
#include <KItinerary/Reservation>
#include <KItinerary/Uic9183Parser>

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

MainWindow::Type MainWindow::typeFromName(const QString &name)
{
    const auto me = QMetaEnum::fromType<Type>();
    Q_ASSERT(me.isValid());

    bool ok = false;
    const auto value = me.keyToValue(name.toUtf8().constData(), &ok);
    if (ok) {
        return static_cast<Type>(value);
    }

    for (auto i = 0; i < me.keyCount(); ++i) {
        if (qstricmp(name.toUtf8().constData(), me.key(i)) == 0) {
            return static_cast<Type>(me.value(i));
        }
    }

    return {};
}

void MainWindow::openFile(const QString &file)
{
    ui->fileRequester->setText(file);
}

void MainWindow::sourceChanged()
{
    m_engine.clear();
    m_imageModel->removeRows(0, m_imageModel->rowCount());
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

    KMime::Message contextMsg; // TODO lifetime!
    contextMsg.from()->fromUnicodeString(ui->senderBox->currentText(), "utf-8");
    contextMsg.date()->setDateTime(ui->contextDate->dateTime());
    m_engine.setContext(QVariant::fromValue<KMime::Content*>(&contextMsg), u"message/rfc822");

    m_engine.setData(m_data, ui->fileRequester->url().toString());
    const auto data = m_engine.extract();
    ui->extractorWidget->showExtractor(m_engine.usedCustomExtractor());

    m_extractorDocModel->setRootNode(m_engine.rootDocumentNode());
    ui->documentTreeView->expandAll();
    setCurrentDocumentNode(m_engine.rootDocumentNode());

    m_outputDoc->setReadWrite(true);
    m_outputDoc->setText(QJsonDocument(data).toJson());
    m_outputDoc->setReadWrite(false);

    ExtractorPostprocessor postproc;
    postproc.setContextDate(ui->contextDate->dateTime());
    postproc.setValidationEnabled(false);
    postproc.process(JsonLdDocument::fromJson(data));
    auto result = postproc.result();

    m_postprocDoc->setReadWrite(true);
    m_postprocDoc->setText(QJsonDocument(JsonLdDocument::toJson(result)).toJson());
    m_postprocDoc->setReadWrite(false);

    ExtractorValidator validator;
    validator.setAcceptOnlyCompleteElements(ui->acceptCompleteOnly->isChecked());
    result.erase(std::remove_if(result.begin(), result.end(), [&validator](const auto &elem) {
        return !validator.isValidElement(elem);
    }), result.end());
    m_validatedDoc->setReadWrite(true);
    m_validatedDoc->setText(QJsonDocument(JsonLdDocument::toJson(result)).toJson());
    m_validatedDoc->setReadWrite(false);

    const auto batches = batchReservations(postproc.result());
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

    if (url.isLocalFile()) {
        QFile f(url.toLocalFile());
        f.open(QFile::ReadOnly);
        m_data = f.readAll();

        const auto isText = std::none_of(m_data.begin(), m_data.end(), [](char c) { return std::iscntrl(c) && !std::isspace(c); });
        if (isText) {
            m_sourceDoc->openUrl(url);
            m_sourceView->show();
        } else {
            m_sourceView->hide();
            sourceChanged();
        }
    } else { // remote content: in theory we'd need to check for binary data there as well...
        m_sourceDoc->openUrl(url);
        m_sourceView->show();
    }
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
            const auto code = decoder.decodeString(idx.data(Qt::DecorationRole).value<QImage>());
            QGuiApplication::clipboard()->setText(code);
        } else if (action == barcodeBinary) {
            BarcodeDecoder decoder;
            const auto b = decoder.decodeBinary(idx.data(Qt::DecorationRole).value<QImage>());
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
    ui->inputTabWidget->setTabEnabled(TextTab, false);
    ui->inputTabWidget->setTabEnabled(ImageTab, false);
    ui->inputTabWidget->setTabEnabled(DomTab, false);
    ui->inputTabWidget->setTabEnabled(Uic9183Tab, false);

    m_imageModel->removeRows(0, m_imageModel->rowCount());
    m_domModel->setDocument(nullptr);

    using namespace KItinerary;
    m_currentNode = node;

    if (node.mimeType() == QLatin1String("application/pdf")) {
        const auto pdf = node.content<PdfDocument*>();
        m_preprocDoc->setText(pdf->text());

        for (int i = 0; i < pdf->pageCount(); ++i) {
            auto pageItem = new QStandardItem;
            pageItem->setText(i18n("Page %1", i + 1));
            const auto page = pdf->page(i);
            for (int j = 0; j < page.imageCount(); ++j) {
                auto imgItem = new QStandardItem;
                const auto img = page.image(j);
                imgItem->setData(img.image(), Qt::DecorationRole);
                imgItem->setToolTip(i18n("Size: %1 x %2\nSource: %3 x %4", img.width(), img.height(), img.sourceWidth(), img.sourceHeight()));
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
}
