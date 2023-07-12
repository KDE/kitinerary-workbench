/*
    SPDX-FileCopyrightText: 2016 Volker Krause <vkrause@kde.org>
    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef METAENUMCOMBOBOX_H
#define METAENUMCOMBOBOX_H

#include <QComboBox>

class MetaEnumComboBox : public QComboBox
{
    Q_OBJECT
    Q_PROPERTY(QVariant value READ value WRITE setValue USER true)
public:
    explicit MetaEnumComboBox(QWidget *parent = nullptr);
    ~MetaEnumComboBox() override;

    QVariant value() const;
    void setValue(const QVariant &value);

private:
    QVariant m_value;
};

#endif // METAENUMCOMBOBOX_H
