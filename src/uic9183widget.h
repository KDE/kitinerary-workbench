/*
    SPDX-FileCopyrightText: 2019 Volker Krause <vkrause@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef UIC9183WIDGET_H
#define UIC9183WIDGET_H

#include <QWidget>

#include <memory>

namespace Ui {
class Uic9183Widget;
}

namespace KItinerary {
class Uic9183Parser;
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
    std::unique_ptr<Ui::Uic9183Widget> ui;

    QStandardItemModel *m_uic9183BlockModel;
    Uic9183TicketLayoutModel *m_ticketLayoutModel;
    QStandardItemModel *m_vendor0080BLModel;
};

#endif // UIC9183WIDGET_H
