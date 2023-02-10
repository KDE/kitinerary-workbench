/*
    SPDX-FileCopyrightText: 2019 Volker Krause <vkrause@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "extractoreditorwidget.h"
#include "ui_extractoreditorwidget.h"

#include <KItinerary/ExtractorFilter>
#include <KItinerary/ExtractorRepository>
#include <KItinerary/ScriptExtractor>

#include <KTextEditor/Document>
#include <KTextEditor/Editor>
#include <KTextEditor/View>

#include <KActionCollection>
#include <KLocalizedString>

#include <QAbstractTableModel>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QFileDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QMetaEnum>
#include <QSettings>

using namespace KItinerary;

class ExtractorFilterModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit ExtractorFilterModel(QObject *parent = nullptr);
    ~ExtractorFilterModel() = default;

    const std::vector<ExtractorFilter>& filters() const;
    void setFilters(std::vector<ExtractorFilter> filters);
    void addFilter();
    void removeFilter(int row);

    bool isReadOnly() const;
    void setReadOnly(bool ro);

    int columnCount(const QModelIndex &parent = {}) const override;
    int rowCount(const QModelIndex &parent = {}) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

private:
    std::vector<ExtractorFilter> m_filters;
    bool m_readOnly = false;
};

ExtractorFilterModel::ExtractorFilterModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

const std::vector<ExtractorFilter>& ExtractorFilterModel::filters() const
{
    return m_filters;
}

void ExtractorFilterModel::setFilters(std::vector<ExtractorFilter> filters)
{
    beginResetModel();
    m_filters = std::move(filters);
    endResetModel();
}

void ExtractorFilterModel::addFilter()
{
    beginInsertRows({}, m_filters.size(), m_filters.size());
    ExtractorFilter f;
    f.setMimeType(QStringLiteral("text/plain"));
    f.setFieldName(i18n("<field>"));
    f.setPattern(i18n("<pattern>"));
    m_filters.push_back(f);
    endInsertRows();
}

void ExtractorFilterModel::removeFilter(int row)
{
    beginRemoveRows({}, row, row);
    m_filters.erase(m_filters.begin() + row);
    endRemoveRows();
}

bool ExtractorFilterModel::isReadOnly() const
{
    return m_readOnly;
}

void ExtractorFilterModel::setReadOnly(bool ro)
{
    m_readOnly = ro;
    if (!m_filters.empty()) {
        Q_EMIT dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1));
    }
}

int ExtractorFilterModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 4;
}

int ExtractorFilterModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_filters.size();
}

Qt::ItemFlags ExtractorFilterModel::flags(const QModelIndex &index) const
{
    if (m_readOnly) {
        return QAbstractTableModel::flags(index);
    }
    return QAbstractTableModel::flags(index) | Qt::ItemIsEditable;
}

QVariant ExtractorFilterModel::data(const QModelIndex &index, int role) const
{
    if (role == Qt::DisplayRole) {
        const auto &filter = m_filters[index.row()];
        switch (index.column()) {
            case 0: return filter.mimeType();
            case 1: return QString::fromUtf8(QMetaEnum::fromType<ExtractorFilter::Scope>().valueToKey(filter.scope()));
            case 2: return filter.fieldName();
            case 3: return filter.pattern();
        }
    } else if (role == Qt::EditRole) {
        const auto &filter = m_filters[index.row()];
        switch (index.column()) {
            case 0: return filter.mimeType();
            case 1: return filter.scope();
            case 2: return filter.fieldName();
            case 3: return filter.pattern();
        }
    }
    return {};
}

bool ExtractorFilterModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || role != Qt::EditRole) {
        return false;
    }

    auto &filter = m_filters[index.row()];
    switch (index.column()) {
        case 0:
            filter.setMimeType(value.toString());
            break;
        case 1:
            filter.setScope(static_cast<ExtractorFilter::Scope>(value.toInt()));
            break;
        case 2:
            filter.setFieldName(value.toString());
            break;
        case 3:
            filter.setPattern(value.toString());
            break;
    }

    Q_EMIT dataChanged(index, index);
    return true;
}

QVariant ExtractorFilterModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
        switch (section) {
            case 0: return i18n("Type");
            case 1: return i18n("Scope");
            case 2: return i18n("Value");
            case 3: return i18n("Pattern");
        }
    }
    return QAbstractTableModel::headerData(section, orientation, role);
}


ExtractorEditorWidget::ExtractorEditorWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ExtractorEditorWidget)
    , m_filterModel(new ExtractorFilterModel(this))
{
    ui->setupUi(this);
    ui->inputType->addItems({
        QStringLiteral("application/ld+json"),
        QStringLiteral("application/octet-stream"),
        QStringLiteral("application/pdf"),
        QStringLiteral("application/vnd.apple.pkpass"),
        QStringLiteral("internal/era-elb"),
        QStringLiteral("internal/era-ssb"),
        QStringLiteral("internal/event"),
        QStringLiteral("internal/uic9183"),
        QStringLiteral("internal/vdv"),
        QStringLiteral("message/rfc822"),
        QStringLiteral("text/calendar"),
        QStringLiteral("text/html"),
        QStringLiteral("text/plain")
    });
    ui->filterView->setModel(m_filterModel);

    QSettings settings;
    settings.beginGroup(QLatin1String("Extractor Repository"));
    ExtractorRepository repo;
    repo.setAdditionalSearchPaths(settings.value(QLatin1String("SearchPaths"), QStringList()).toStringList());
    repo.reload();

    connect(ui->extractorCombobox, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() {
        ExtractorRepository repo;
        const auto extId = ui->extractorCombobox->currentText();
        const auto extractor = dynamic_cast<const ScriptExtractor*>(repo.extractorByName(extId));
        if (!extractor) {
            return;
        }
        ui->scriptEdit->setText(extractor->scriptFileName());
        ui->functionEdit->setText(extractor->scriptFunction());
        ui->inputType->setCurrentIndex(ui->inputType->findText(extractor->mimeType()));
        m_filterModel->setFilters(extractor->filters());
        m_scriptDoc->openUrl(QUrl::fromLocalFile(extractor->scriptFileName()));

        QFileInfo scriptFi(extractor->fileName());
        m_scriptDoc->setReadWrite(scriptFi.isWritable());
        QFileInfo metaFi(extractor->fileName());
        setMetaDataReadOnly(!metaFi.isWritable());
        validateInput();
    });

    auto editor = KTextEditor::Editor::instance();
    m_scriptDoc = editor->createDocument(nullptr);
    m_scriptDoc->setHighlightingMode(QStringLiteral("JavaScript"));
    m_scriptView = m_scriptDoc->createView(nullptr);
    ui->topLayout->addWidget(m_scriptView);
    reloadExtractors();

    connect(m_scriptDoc, &KTextEditor::Document::modifiedChanged, this, [this]() {
        if (!m_scriptDoc->isModified()) { // approximation for "document has been saved"
            Q_EMIT extractorChanged();
        }
    });

    connect(ui->addFilterButton, &QToolButton::clicked, m_filterModel, &ExtractorFilterModel::addFilter);
    connect(ui->removeFilterButton, &QToolButton::clicked, this, [this]() {
        const auto sel = ui->filterView->selectionModel()->selection();
        if (sel.isEmpty()) {
            return;
        }
        m_filterModel->removeFilter(sel.first().topLeft().row());
        validateInput();
    });

    connect(ui->scriptEdit, &QLineEdit::textChanged, this, &ExtractorEditorWidget::validateInput);
    connect(ui->functionEdit, &QLineEdit::textChanged, this, &ExtractorEditorWidget::validateInput);
    connect(m_filterModel, &ExtractorFilterModel::rowsInserted, this, &ExtractorEditorWidget::validateInput);
    connect(m_filterModel, &ExtractorFilterModel::dataChanged, this, &ExtractorEditorWidget::validateInput);
    connect(ui->filterView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &ExtractorEditorWidget::validateInput);
}

ExtractorEditorWidget::~ExtractorEditorWidget() = default;

void ExtractorEditorWidget::registerActions(KActionCollection *ac)
{
    connect(ui->actionFileNewExtractor, &QAction::triggered, this, &ExtractorEditorWidget::create);
    connect(ui->actionFileSaveExtractor, &QAction::triggered, this, &ExtractorEditorWidget::save);

    ac->addAction(QStringLiteral("file_new_extractor"), ui->actionFileNewExtractor);
    ac->addAction(QStringLiteral("file_save_extractor"), ui->actionFileSaveExtractor);
}

void ExtractorEditorWidget::reloadExtractors()
{
    ui->extractorCombobox->clear();
    ExtractorRepository repo;
    for (const auto &ext : repo.extractors()) {
        if (dynamic_cast<ScriptExtractor*>(ext.get())) {
            ui->extractorCombobox->addItem(ext->name());
        }
    }
}

void ExtractorEditorWidget::showExtractor(const QString &extractorId)
{
    const auto idx = ui->extractorCombobox->findText(extractorId);
    if (idx >= 0 && idx != ui->extractorCombobox->currentIndex()) {
        ui->extractorCombobox->setCurrentIndex(idx);
    }
}

void ExtractorEditorWidget::navigateToSource(const QString &fileName, int line)
{
    // TODO find the extractor this file belongs to and select it?
    if (m_scriptDoc->url().toString() != fileName) {
        m_scriptDoc->openUrl(QUrl(fileName));
    }
    m_scriptView->setCursorPosition(KTextEditor::Cursor(line - 1, 0));
}

void ExtractorEditorWidget::setMetaDataReadOnly(bool readOnly)
{
    ui->inputType->setEnabled(!readOnly);
    ui->scriptEdit->setEnabled(!readOnly);
    ui->functionEdit->setEnabled(!readOnly);
    m_filterModel->setReadOnly(readOnly);
    ui->addFilterButton->setEnabled(!readOnly);
    ui->removeFilterButton->setEnabled(!readOnly);
}

void ExtractorEditorWidget::save()
{
    ExtractorRepository repo;
    const auto extId = ui->extractorCombobox->currentText();
    auto extractor = const_cast<ScriptExtractor*>(dynamic_cast<const ScriptExtractor*>(repo.extractorByName(extId)));
    Q_ASSERT(extractor);

    extractor->setMimeType(ui->inputType->currentText());
    extractor->setScriptFileName(ui->scriptEdit->text());
    extractor->setScriptFunction(ui->functionEdit->text());
    extractor->setFilters(m_filterModel->filters());

    const auto val = repo.extractorToJson(extractor);

    QFile f(extractor->fileName());
    if (!f.open(QFile::WriteOnly)) {
        QMessageBox::critical(this, i18n("Saving Failed"), i18n("Failed to open file %1 for saving: %2", f.fileName(), f.errorString()));
        return;
    }
    f.write((val.isArray() ? QJsonDocument(val.toArray()) : QJsonDocument(val.toObject())).toJson());
    f.close();
    m_scriptDoc->save();

    repo.reload();
}

void ExtractorEditorWidget::create()
{
    ExtractorRepository repo;
    QString startDir;
    if (!repo.additionalSearchPaths().empty()) {
        startDir = repo.additionalSearchPaths().at(0);
    }

    const auto metaFileName = QFileDialog::getSaveFileName(this, i18n("Create New Extractor"), startDir, i18n("JSON (*.json)"), nullptr, QFileDialog::DontConfirmOverwrite);
    if (metaFileName.isEmpty()) {
        return;
    }

    QFileInfo metaFi(metaFileName);
    const QString scriptFileName = metaFi.path() + QLatin1Char('/') + metaFi.baseName() + QLatin1String(".js");
    QFile scriptFile(scriptFileName);
    if (!scriptFile.open(QFile::WriteOnly | QFile::Append)) {
        QMessageBox::critical(this, i18n("Creation Failed"), i18n("Failed to create file %1: %2", scriptFile.fileName(), scriptFile.errorString()));
        return;
    }
    scriptFile.write(R"(
function main(content) {
    console.log(content);
}
)");
    scriptFile.close();

    ScriptExtractor extractor;
    extractor.load({}, metaFileName, std::numeric_limits<int>::max()); // use a certainly unused index, so this doesn't clash with existing ones in a multi-extractor file
    extractor.setScriptFileName(scriptFileName);
    extractor.setMimeType(QStringLiteral("text/plain"));
    ExtractorFilter filter;
    filter.setMimeType(QStringLiteral("message/rfc822"));
    filter.setFieldName(QStringLiteral("From"));
    filter.setPattern(QStringLiteral("@change-me.com"));
    extractor.setFilters({filter});
    QFile metaFile(metaFileName);
    if (!metaFile.open(QFile::WriteOnly)) {
        QMessageBox::critical(this, i18n("Creation Failed"), i18n("Failed to create file %1: %2", metaFile.fileName(), metaFile.errorString()));
        return;
    }
    const auto json = repo.extractorToJson(&extractor);
    metaFile.write((json.isArray() ? QJsonDocument(json.toArray()) : QJsonDocument(json.toObject())).toJson());
    metaFile.close();

    repo.reload();
    reloadExtractors();
    showExtractor(metaFi.baseName());
}

void ExtractorEditorWidget::validateInput()
{
    bool valid = !ui->scriptEdit->text().isEmpty() && !ui->functionEdit->text().isEmpty();
    ui->actionFileSaveExtractor->setEnabled(valid);
    ui->removeFilterButton->setEnabled(m_filterModel->rowCount() > 1 && !m_filterModel->isReadOnly());
}

#include "extractoreditorwidget.moc"
