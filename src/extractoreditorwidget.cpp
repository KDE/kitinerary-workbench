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
#include <KItinerary/ExtractorFilter>
#include <KItinerary/ExtractorRepository>

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

    int columnCount(const QModelIndex &parent) const override;
    int rowCount(const QModelIndex &parent) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

private:
    std::vector<ExtractorFilter> m_filters;
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
    f.setType(ExtractorInput::Text);
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

int ExtractorFilterModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 3;
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
    return QAbstractTableModel::flags(index) | Qt::ItemIsEditable;
}

QVariant ExtractorFilterModel::data(const QModelIndex &index, int role) const
{
    if (role == Qt::DisplayRole) {
        const auto &filter = m_filters[index.row()];
        switch (index.column()) {
            case 0: return ExtractorInput::typeToString(filter.type());
            case 1: return filter.fieldName();
            case 2: return filter.pattern();
        }
    } else if (role == Qt::EditRole) {
        const auto &filter = m_filters[index.row()];
        switch (index.column()) {
            case 0: return filter.type();
            case 1: return filter.fieldName();
            case 2: return filter.pattern();
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
            filter.setType(static_cast<ExtractorInput::Type>(value.toInt()));
            break;
        case 1:
            filter.setFieldName(value.toString());
            break;
        case 2:
            filter.setPattern(value.toString());
            break;
    }

    emit dataChanged(index, index);
    return true;
}

QVariant ExtractorFilterModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
        switch (section) {
            case 0: return i18n("Type");
            case 1: return i18n("Value");
            case 2: return i18n("Pattern");
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
    const auto me = ExtractorInput::staticMetaObject.enumerator(0);
    for (int i = 0; i < me.keyCount(); ++i) {
        ui->inputType->addItem(QString::fromUtf8(me.key(i)), me.value(i));
    }
    ui->filterView->setModel(m_filterModel);

    QSettings settings;
    settings.beginGroup(QLatin1String("Extractor Repository"));
    ExtractorRepository repo;
    repo.setAdditionalSearchPaths(settings.value(QLatin1String("SearchPaths"), QStringList()).toStringList());
    repo.reload();

    connect(ui->extractorCombobox, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() {
        ExtractorRepository repo;
        const auto extId = ui->extractorCombobox->currentText();
        const auto extractor = repo.extractor(extId);
        ui->scriptEdit->setText(extractor.scriptFileName());
        ui->functionEdit->setText(extractor.scriptFunction());
        ui->inputType->setCurrentIndex(ui->inputType->findData(extractor.type()));
        m_filterModel->setFilters(extractor.filters());
        m_scriptDoc->openUrl(QUrl::fromLocalFile(extractor.scriptFileName()));

        QFileInfo scriptFi(extractor.fileName());
        m_scriptDoc->setReadWrite(scriptFi.isWritable());
        QFileInfo metaFi(extractor.fileName());
        setMetaDataReadOnly(!metaFi.isWritable() || extractor.name().contains(QLatin1Char(':')));
    });

    auto editor = KTextEditor::Editor::instance();
    m_scriptDoc = editor->createDocument(nullptr);
    m_scriptDoc->setHighlightingMode(QStringLiteral("JavaScript"));
    m_scriptView = m_scriptDoc->createView(nullptr);
    ui->topLayout->addWidget(m_scriptView);
    reloadExtractors();

    connect(m_scriptDoc, &KTextEditor::Document::modifiedChanged, this, [this]() {
        if (!m_scriptDoc->isModified()) { // approximation for "document has been saved"
            emit extractorChanged();
        }
    });

    connect(ui->addFilterButton, &QToolButton::clicked, m_filterModel, &ExtractorFilterModel::addFilter);
    connect(ui->removeFilterButton, &QToolButton::clicked, this, [this]() {
        const auto sel = ui->filterView->selectionModel()->selection();
        if (sel.isEmpty()) {
            return;
        }
        m_filterModel->removeFilter(sel.first().topLeft().row());
    });
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

void ExtractorEditorWidget::navigateToSource(const QString &fileName, int line)
{
    // TODO find the extractor this file belongs to and select it?
    if (m_scriptDoc->url().toString() != fileName) {
        m_scriptDoc->openUrl(fileName);
    }
    m_scriptView->setCursorPosition(KTextEditor::Cursor(line - 1, 0));
}

void ExtractorEditorWidget::setMetaDataReadOnly(bool readOnly)
{
    ui->inputType->setEnabled(!readOnly);
    ui->scriptEdit->setEnabled(!readOnly);
    ui->functionEdit->setEnabled(!readOnly);
    ui->filterView->setEnabled(!readOnly);
}

void ExtractorEditorWidget::save()
{
    ExtractorRepository repo;
    const auto extId = ui->extractorCombobox->currentText();
    auto extractor = repo.extractor(extId);

    extractor.setType(static_cast<ExtractorInput::Type>(ui->inputType->currentData().toInt()));
    extractor.setScriptFileName(ui->scriptEdit->text());
    extractor.setScriptFunction(ui->functionEdit->text());
    extractor.setFilters(m_filterModel->filters());

    const auto obj = extractor.toJson();

    QFile f(extractor.fileName());
    if (!f.open(QFile::WriteOnly)) {
        QMessageBox::critical(this, i18n("Saving Failed"), i18n("Failed to open file %1 for saving: %2", f.fileName(), f.errorString()));
        return;
    }
    f.write(QJsonDocument(obj).toJson());
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

    const auto metaFileName = QFileDialog::getSaveFileName(this, i18n("Create New Extractor"), startDir, QStringLiteral("*.json"));
    if (metaFileName.isEmpty()) {
        return;
    }

    QFileInfo metaFi(metaFileName);
    const QString scriptFileName = metaFi.path() + QLatin1Char('/') + metaFi.baseName() + QLatin1String(".js");
    QFile scriptFile(scriptFileName);
    if (!scriptFile.open(QFile::WriteOnly)) {
        QMessageBox::critical(this, i18n("Creation Failed"), i18n("Failed to create file %1: %2", scriptFile.fileName(), scriptFile.errorString()));
        return;
    }
    scriptFile.write(
R"(function main(content) {
    console.log(content);
})");
    scriptFile.close();

    Extractor extractor;
    extractor.load({}, metaFileName);
    extractor.setScriptFileName(scriptFileName);
    ExtractorFilter filter;
    filter.setType(ExtractorInput::Email);
    filter.setFieldName(QStringLiteral("From"));
    filter.setPattern(QStringLiteral("@change-me.com"));
    extractor.setFilters({filter});
    QFile metaFile(metaFileName);
    if (!metaFile.open(QFile::WriteOnly)) {
        QMessageBox::critical(this, i18n("Creation Failed"), i18n("Failed to create file %1: %2", metaFile.fileName(), metaFile.errorString()));
        return;
    }
    metaFile.write(QJsonDocument(extractor.toJson()).toJson());
    metaFile.close();

    repo.reload();
    reloadExtractors();
    showExtractor(metaFi.baseName());
}

#include "extractoreditorwidget.moc"
