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

#include "consoleoutputwidget.h"
#include "ui_consoleoutputwidget.h"

#include <KLocalizedString>

#include <QAbstractTableModel>
#include <QIcon>

#include <cstring>

class ConsoleOutputModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit ConsoleOutputModel(QObject *parent = nullptr);
    ~ConsoleOutputModel();

    enum Role {
        SourceFileRole = Qt::UserRole,
        SourceLineRole
    };

    int columnCount(const QModelIndex &parent) const override;
    int rowCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    void clear();

    void handleMessage(QtMsgType type, const QMessageLogContext &context, const QString &msg);

private:
    struct Message {
        QString msg;
        QString file;
        QString function;
        QtMsgType type;
        int line;
    };
    std::vector<Message> m_messages;
    QtMessageHandler m_prevHandler = nullptr;
};

static ConsoleOutputModel *sConsoleOutput = nullptr;

void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    sConsoleOutput->handleMessage(type, context, msg);
}

ConsoleOutputModel::ConsoleOutputModel(QObject *parent)
    : QAbstractTableModel(parent)
{
    sConsoleOutput = this;
    m_prevHandler = qInstallMessageHandler(messageHandler);
}

ConsoleOutputModel::~ConsoleOutputModel()
{
    qInstallMessageHandler(0);
    sConsoleOutput = nullptr;
}

int ConsoleOutputModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 3;
}

int ConsoleOutputModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_messages.size();
}

QVariant ConsoleOutputModel::data(const QModelIndex& index, int role) const
{
    if (role == Qt::DisplayRole) {
        const auto &msg = m_messages[index.row()];
        switch (index.column()) {
            case 0:
                return i18n("%1:%2", msg.file, msg.line);
            case 1:
                return msg.function;
            case 2:
                return msg.msg;
        }
    }

    if (role == Qt::DecorationRole && index.column() == 0) {
        switch (m_messages[index.row()].type) {
            case QtDebugMsg: return QIcon::fromTheme(QStringLiteral("dialog-question"));
            case QtInfoMsg: return QIcon::fromTheme(QStringLiteral("dialog-information"));
            case QtWarningMsg: return QIcon::fromTheme(QStringLiteral("dialog-warning"));
            case QtCriticalMsg: return QIcon::fromTheme(QStringLiteral("dialog-error"));
            case QtFatalMsg: return QIcon::fromTheme(QStringLiteral("tools-report-bug"));
        }
    }

    if (role == SourceFileRole) {
        return m_messages[index.row()].file;
    }
    if (role == SourceLineRole) {
        return m_messages[index.row()].line;
    }

    return {};
}

QVariant ConsoleOutputModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
        switch (section) {
            case 0: return i18n("Location");
            case 1: return i18n("Function");
            case 2: return i18n("Message");
        }
    }
    return QAbstractTableModel::headerData(section, orientation, role);
}

void ConsoleOutputModel::clear()
{
    if (m_messages.empty()) {
        return;
    }

    beginRemoveRows({}, 0, m_messages.size() - 1);
    m_messages.clear();
    endRemoveRows();
}

void ConsoleOutputModel::handleMessage(QtMsgType type, const QMessageLogContext& context, const QString& msg)
{
    m_prevHandler(type, context, msg);
    if (std::strcmp(context.category, "js") != 0) { // we only care for the script output
        return;
    }

    beginInsertRows({}, m_messages.size(), m_messages.size());
    Message m;
    m.msg = msg;
    m.file = QString::fromUtf8(context.file);
    m.function = QString::fromUtf8(context.function);
    m.line = context.line;
    m.type = type;
    m_messages.push_back(std::move(m));
    endInsertRows();
}


ConsoleOutputWidget::ConsoleOutputWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ConsoleOutputWidget)
    , m_model(new ConsoleOutputModel(this))
{
    ui->setupUi(this);
    ui->logView->setModel(m_model);

    connect(ui->logView, &QTreeView::activated, this, [this](const auto &idx) {
        if (!idx.isValid()) {
            return;
        }
        const auto file = idx.data(ConsoleOutputModel::SourceFileRole).toString();
        const auto line = idx.data(ConsoleOutputModel::SourceLineRole).toInt();
        emit navigateToSource(file, line);
    });
}

ConsoleOutputWidget::~ConsoleOutputWidget() = default;

void ConsoleOutputWidget::clear()
{
    m_model->clear();
}

#include "consoleoutputwidget.moc"
