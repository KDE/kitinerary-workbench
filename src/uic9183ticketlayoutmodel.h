/*
    SPDX-FileCopyrightText: 2019 Volker Krause <vkrause@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef UIC9183TICKETLAYOUTMODEL_H
#define UIC9183TICKETLAYOUTMODEL_H

#include <KItinerary/Uic9183TicketLayout>

#include <QAbstractTableModel>

/** Model showing a U_TLAY block of an UIC 918-3 ticket. */
class Uic9183TicketLayoutModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit Uic9183TicketLayoutModel(QObject *parent = nullptr);
    ~Uic9183TicketLayoutModel();

    void setLayout(const KItinerary::Uic9183TicketLayout &layout);

    QStringList supportedTemplates();
    void setLayoutTemplate(int tplIndex);

    int columnCount(const QModelIndex &parent) const override;
    int rowCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

private:
    KItinerary::Uic9183TicketLayout m_layout;
    int m_layoutTemplate = -1;
};

#endif // UIC9183TICKETLAYOUTMODEL_H
