/*
    SPDX-FileCopyrightText: 2018 Volker Krause <vkrause@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <KItinerary/ExtractorDocumentNode>
#include <KItinerary/ExtractorEngine>

#include <KXmlGuiWindow>

#include <memory>

namespace KMime {
class Message;
}

namespace KTextEditor {
class Document;
class View;
}

namespace Ui
{
class MainWindow;
}

class AttributeModel;
class DocumentModel;
class DOMModel;
class QStandardItemModel;

class MainWindow : public KXmlGuiWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void openFile(const QString &file);

private:
    enum InputTab {
        ExtractorEditorTab = 0,
        InputTab = 1,
        DocumentTab = 2,
        TextTab = 3,
        ImageTab = 4,
        DomTab = 5,
        Uic9183Tab = 6,
        IataBcbpTab = 7,
        EraSsbTab = 8,
        VdvTab = 9,
    };

    enum OutputTab {
        ExtractorOutputTab = 0,
        PostprocessorTab = 1,
        ValidatedTab = 2,
        ICalTab = 3,
        ConsoleTab = 4
    };

    void typeChanged();
    void sourceChanged();
    void urlChanged();
    void loadFromClipboard();
    void imageContextMenu(QPoint pos);
    void setCurrentDocumentNode(const KItinerary::ExtractorDocumentNode &node);

private:
    std::unique_ptr<Ui::MainWindow> ui;

    KTextEditor::Document *m_sourceDoc = nullptr;
    KTextEditor::Document *m_preprocDoc = nullptr;
    KTextEditor::Document *m_outputDoc = nullptr;
    KTextEditor::Document *m_postprocDoc = nullptr;
    KTextEditor::Document *m_validatedDoc = nullptr;
    KTextEditor::Document *m_icalDoc = nullptr;
    KTextEditor::View *m_sourceView = nullptr;

    DocumentModel *m_extractorDocModel;
    QStandardItemModel *m_imageModel;
    DOMModel *m_domModel;
    AttributeModel *m_attrModel;
    QStandardItemModel *m_iataBcbpModel;
    QStandardItemModel *m_eraSsbModel;
    QStandardItemModel *m_vdvModel;

    KItinerary::ExtractorEngine m_engine;
    QByteArray m_data;
    std::unique_ptr<KMime::Message> m_contextMsg;
    KItinerary::ExtractorDocumentNode m_currentNode;
};

#endif // MAINWINDOW_H
