set(workbench_srcs
    main.cpp
    mainwindow.cpp
)

add_executable(kitinerary-workbench ${workbench_srcs})
target_link_libraries(kitinerary-workbench
    KPim::Itinerary
    KF5::TextEditor
)
