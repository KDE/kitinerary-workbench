/*
    SPDX-FileCopyrightText: 2019 Volker Krause <vkrause@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef EXTRACTOREDITORWIDGET_H
#define EXTRACTOREDITORWIDGET_H

#include <QWidget>

#include <memory>

namespace KTextEditor {
class Document;
class View;
}

class KActionCollection;

class Ui_ExtractorEditorWidget;
class ExtractorFilterModel;

class ExtractorEditorWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ExtractorEditorWidget(QWidget *parent = nullptr);
    ~ExtractorEditorWidget();
    void registerActions(KActionCollection *ac);

    void showExtractor(const QString &extractorId);
    void navigateToSource(const QString &fileName, int line);
    void reloadExtractors();

signals:
    void extractorChanged();

private:
    void setMetaDataReadOnly(bool readOnly);
    void save();
    void create();
    void validateInput();

    std::unique_ptr<Ui_ExtractorEditorWidget> ui;
    ExtractorFilterModel *m_filterModel = nullptr;

    KTextEditor::Document *m_scriptDoc = nullptr;
    KTextEditor::View *m_scriptView = nullptr;
};

#endif // EXTRACTOREDITORWIDGET_H
