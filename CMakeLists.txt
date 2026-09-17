cmake_minimum_required(VERSION 3.19)
project(Seismic_Waveforms_demo LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets)

include(CTest)

qt_standard_project_setup()

qt_add_executable(Seismic_Waveforms_demo
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    eventlogic.cpp
    eventlogic.h
    waveformmodel.cpp
    waveformmodel.h
)

target_link_libraries(Seismic_Waveforms_demo
    PRIVATE
        Qt::Core
        Qt::Widgets
)

install(TARGETS Seismic_Waveforms_demo
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET Seismic_Waveforms_demo
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})

if(BUILD_TESTING)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Test)

    add_executable(test_waveformmodel
        tests/test_waveformmodel.cpp
        waveformmodel.cpp
        waveformmodel.h
    )
    target_include_directories(test_waveformmodel PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(test_waveformmodel PRIVATE Qt::Core Qt::Test)
    set_target_properties(test_waveformmodel PROPERTIES WIN32_EXECUTABLE FALSE)
    add_test(NAME waveformmodel COMMAND test_waveformmodel)

    add_executable(test_eventlogic
        tests/test_eventlogic.cpp
        eventlogic.cpp
        eventlogic.h
    )
    target_include_directories(test_eventlogic PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(test_eventlogic PRIVATE Qt::Core Qt::Test)
    set_target_properties(test_eventlogic PROPERTIES WIN32_EXECUTABLE FALSE)
    add_test(NAME eventlogic COMMAND test_eventlogic)

    add_executable(test_waveformwidgets
        tests/test_waveformwidgets.cpp
        mainwindow.cpp
        mainwindow.h
        eventlogic.cpp
        eventlogic.h
        waveformmodel.cpp
        waveformmodel.h
    )
    target_include_directories(test_waveformwidgets PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(test_waveformwidgets PRIVATE Qt::Core Qt::Widgets Qt::Test)
    set_target_properties(test_waveformwidgets PROPERTIES WIN32_EXECUTABLE FALSE)
    add_test(NAME waveformwidgets COMMAND test_waveformwidgets)

    add_executable(test_mainwindow
        tests/test_mainwindow.cpp
        mainwindow.cpp
        mainwindow.h
        eventlogic.cpp
        eventlogic.h
        waveformmodel.cpp
        waveformmodel.h
    )
    target_include_directories(test_mainwindow PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(test_mainwindow PRIVATE Qt::Core Qt::Widgets Qt::Test)
    set_target_properties(test_mainwindow PROPERTIES WIN32_EXECUTABLE FALSE)
    add_test(NAME mainwindow COMMAND test_mainwindow)

    set_tests_properties(waveformmodel eventlogic waveformwidgets mainwindow PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=minimal;QT_PLUGIN_PATH=D:/develop/Qt/6.11.2/mingw_64/plugins"
        ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:D:/develop/Qt/Tools/mingw1310_64/bin;PATH=path_list_prepend:D:/develop/Qt/6.11.2/mingw_64/bin"
    )
endif()
