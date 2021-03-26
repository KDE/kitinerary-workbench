/*
    SPDX-FileCopyrightText: 2019 Volker Krause <vkrause@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <KItinerary/ExtractorRepository>

#include <QDialog>

#include <memory>

class QStringListModel;

namespace Ui
{
class SettingsDialog;
}

class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    ~SettingsDialog();

    void accept() override;

private:
    std::unique_ptr<Ui::SettingsDialog> ui;
    QStringListModel *m_searchPathModel = nullptr;
};

#endif // SETTINGSDIALOG_H
