/*
    SPDX-FileCopyrightText: 2018 Volker Krause <vkrause@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "mainwindow.h"

#include <QApplication>
#include <QCommandLineParser>

int main(int argc, char **argv)
{
    QCoreApplication::setOrganizationDomain(QStringLiteral("kde.org"));
    QCoreApplication::setOrganizationName(QStringLiteral("KDE"));
    QCoreApplication::setApplicationName(QStringLiteral("kitinerary-workbench"));

    QApplication app(argc, argv);

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("file"), QStringLiteral("Input file to open."));
    parser.process(app);

    auto mainWindow = new MainWindow;
    mainWindow->show();

    if (parser.positionalArguments().size() == 1)
        mainWindow->openFile(parser.positionalArguments().at(0));

    return app.exec();
}
