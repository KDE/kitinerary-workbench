set(workbench_srcs
    main.cpp
    mainwindow.cpp
    attributemodel.cpp
    consoleoutputwidget.cpp
    dommodel.cpp
    extractoreditorwidget.cpp
    settingsdialog.cpp
    uic9183ticketlayoutmodel.cpp
    uic9183widget.cpp
    ui.qrc
)

add_executable(kitinerary-workbench ${workbench_srcs})
target_link_libraries(kitinerary-workbench
    KPim::Itinerary
    KPim::PkPass
    KF5::KIOWidgets
    KF5::TextEditor
)

install(TARGETS kitinerary-workbench ${INSTALL_TARGETS_DEFAULT_ARGS})
install(PROGRAMS org.kde.kitinerary-workbench.desktop DESTINATION ${KDE_INSTALL_APPDIR})
install(FILES org.kde.kitinerary-workbench.appdata.xml DESTINATION ${KDE_INSTALL_METAINFODIR})
