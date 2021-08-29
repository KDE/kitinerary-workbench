/*
    SPDX-FileCopyrightText: 2019 Volker Krause <vkrause@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef UIC9183WIDGET_H
#define UIC9183WIDGET_H

#include <KItinerary/Uic9183Parser>

#include <QWidget>

#include <memory>

namespace Ui {
class Uic9183Widget;
}

class Uic9183TicketLayoutModel;

class QStandardItemModel;


class Uic9183Widget : public QWidget
{
    Q_OBJECT
public:
    explicit Uic9183Widget(QWidget *parent = nullptr);
    ~Uic9183Widget();

    void clear();
    void setContent(const KItinerary::Uic9183Parser &p);

private:
    void blockSelectionChanged();

    std::unique_ptr<Ui::Uic9183Widget> ui;

    KItinerary::Uic9183Parser m_uic9183;
    QStandardItemModel *m_uic9183BlockModel;
    Uic9183TicketLayoutModel *m_ticketLayoutModel;
    QStandardItemModel *m_layoutFieldsModel;
    QStandardItemModel *m_vendor0080BLModel;
    QStandardItemModel *m_vendor0080BLOrderModel;
    QStandardItemModel *m_genericBlockModel;
};

#endif // UIC9183WIDGET_H
