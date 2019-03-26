set(workbench_srcs
    main.cpp
    mainwindow.cpp
    attributemodel.cpp
    dommodel.cpp
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
