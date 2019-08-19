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

#ifndef EXTRACTOREDITORWIDGET_H
#define EXTRACTOREDITORWIDGET_H

#include <QWidget>

#include <memory>

namespace KTextEditor {
class Document;
}

class Ui_ExtractorEditorWidget;

class ExtractorEditorWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ExtractorEditorWidget(QWidget *parent = nullptr);
    ~ExtractorEditorWidget();

    void showExtractor(const QString &extractorId);

signals:
    void extractorChanged();

private:
    void reloadExtractors();

    std::unique_ptr<Ui_ExtractorEditorWidget> ui;

    KTextEditor::Document *m_scriptDoc = nullptr;
};

#endif // EXTRACTOREDITORWIDGET_H
