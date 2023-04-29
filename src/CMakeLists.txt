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
    KPim${KF_MAJOR_VERSION}::Itinerary
    KPim${KF_MAJOR_VERSION}::PkPass
    KF${KF_MAJOR_VERSION}::CalendarCore
    KF${KF_MAJOR_VERSION}::KIOWidgets
    KF${KF_MAJOR_VERSION}::TextEditor
)
if (TARGET Qt6::Core5Compat)
    target_link_libraries(kitinerary-workbench Qt6::Core5Compat) # QTextCodec
endif()

install(TARGETS kitinerary-workbench ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
install(PROGRAMS org.kde.kitinerary-workbench.desktop DESTINATION ${KDE_INSTALL_APPDIR})
install(FILES org.kde.kitinerary-workbench.appdata.xml DESTINATION ${KDE_INSTALL_METAINFODIR})
