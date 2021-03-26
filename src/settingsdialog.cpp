/*
    SPDX-FileCopyrightText: 2019 Volker Krause <vkrause@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "settingsdialog.h"
#include "ui_settingsdialog.h"

#include <KItinerary/ExtractorRepository>

#include <QSettings>
#include <QStringListModel>

using namespace KItinerary;

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::SettingsDialog)
    , m_searchPathModel(new QStringListModel(this))
{
    ui->setupUi(this);

    ui->searchPathView->setModel(m_searchPathModel);

    ExtractorRepository repo;
    m_searchPathModel->setStringList(repo.additionalSearchPaths());

    connect(ui->searchPathAddButton, &QPushButton::clicked, this, [this]() {
        if (!ui->searchPathRequester->url().isValid()) {
            return;
        }
        const auto idx = m_searchPathModel->rowCount();
        m_searchPathModel->insertRows(idx, 1);
        m_searchPathModel->setData(m_searchPathModel->index(idx, 0), ui->searchPathRequester->url().toLocalFile());
        ui->searchPathRequester->clear();
    });

    connect(ui->searchPathRemoveButton, &QPushButton::clicked, this, [this]() {
        const auto sel = ui->searchPathView->selectionModel()->selection();
        if (sel.isEmpty()) {
            return;
        }
        m_searchPathModel->removeRows(sel.first().topLeft().row(), 1);
    });
}

SettingsDialog::~SettingsDialog() = default;

void SettingsDialog::accept()
{
    ExtractorRepository repo;
    repo.setAdditionalSearchPaths(m_searchPathModel->stringList());

    QSettings settings;
    settings.beginGroup(QStringLiteral("Extractor Repository"));
    settings.setValue(QStringLiteral("SearchPaths"), repo.additionalSearchPaths());

    QDialog::accept();
}
