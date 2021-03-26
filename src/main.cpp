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

    QCommandLineOption typeOpt({QStringLiteral("t"), QStringLiteral("type")}, QStringLiteral("Type of the input data [PlainText, Html, Pdf, PkPass, IataBcbp, JsonLd, Uic9183, Image, ICal, Mime, Vdv]."), QStringLiteral("type"));
    parser.addOption(typeOpt);

    parser.addPositionalArgument(QStringLiteral("file"), QStringLiteral("Input file to open."));
    parser.process(app);

    auto mainWindow = new MainWindow;
    mainWindow->show();

    if (parser.isSet(typeOpt)) {
        mainWindow->setType(MainWindow::typeFromName(parser.value(typeOpt)));
    }

    if (parser.positionalArguments().size() == 1)
        mainWindow->openFile(parser.positionalArguments().at(0));

    return app.exec();
}
