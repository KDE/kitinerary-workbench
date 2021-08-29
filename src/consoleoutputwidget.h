/*
    SPDX-FileCopyrightText: 2019 Volker Krause <vkrause@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef CONSOLEOUTPUTWIDGET_H
#define CONSOLEOUTPUTWIDGET_H

#include <QWidget>

#include <memory>

namespace Ui {
class ConsoleOutputWidget;
}
class ConsoleOutputModel;

class ConsoleOutputWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ConsoleOutputWidget(QWidget *parent = nullptr);
    ~ConsoleOutputWidget();

    void clear();

Q_SIGNALS:
    void navigateToSource(const QString &file, int line);

private:
    std::unique_ptr<Ui::ConsoleOutputWidget> ui;
    ConsoleOutputModel *m_model = nullptr;
};

#endif // CONSOLEOUTPUTWIDGET_H
