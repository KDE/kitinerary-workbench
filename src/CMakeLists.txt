# SPDX-FileCopyrightText: 2018-2021 Volker Krause <vkrause@kde.org>
# SPDX-License-Identifier: BSD-3-Clause

set(workbench_srcs
    main.cpp
    mainwindow.cpp
    attributemodel.cpp
    consoleoutputwidget.cpp
    documentmodel.cpp
    dommodel.cpp
    extractoreditorwidget.cpp
    settingsdialog.cpp
    standarditemmodelhelper.cpp
    uic9183ticketlayoutmodel.cpp
    uic9183widget.cpp
    ui.qrc
)

add_executable(kitinerary-workbench ${workbench_srcs})
target_link_libraries(kitinerary-workbench
    KPim::Itinerary
    KPim::PkPass
    KF5::CalendarCore
    KF5::KIOWidgets
    KF5::TextEditor
)

install(TARGETS kitinerary-workbench ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
install(PROGRAMS org.kde.kitinerary-workbench.desktop DESTINATION ${KDE_INSTALL_APPDIR})
install(FILES org.kde.kitinerary-workbench.appdata.xml DESTINATION ${KDE_INSTALL_METAINFODIR})
