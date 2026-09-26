cmake_minimum_required(VERSION 3.19)
project(Seismic_Waveforms_demo VERSION 1.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
configure_file(appversion.h.in ${CMAKE_CURRENT_BINARY_DIR}/generated/appversion.h @ONLY)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Network)

include(CTest)

qt_standard_project_setup()

add_library(app_update STATIC updatemanager.cpp updatemanager.h)
target_include_directories(app_update PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_link_libraries(app_update PUBLIC Qt::Core Qt::Network)

qt_add_executable(Seismic_Waveforms_demo
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    eventlogic.cpp
    eventlogic.h
    spectrumanalyzer.cpp
    spectrumanalyzer.h
    waveformmodel.cpp
    waveformmodel.h
)

target_link_libraries(Seismic_Waveforms_demo
    PRIVATE
        Qt::Core
        Qt::Widgets
        app_update
)
target_include_directories(Seismic_Waveforms_demo PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)

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

    add_executable(test_appversion tests/test_appversion.cpp)
    target_include_directories(test_appversion PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)
    target_link_libraries(test_appversion PRIVATE Qt::Core Qt::Test)
    add_test(NAME appversion COMMAND test_appversion)

    add_executable(test_updatemanager tests/test_updatemanager.cpp)
    target_include_directories(test_updatemanager PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/generated)
    target_link_libraries(test_updatemanager PRIVATE app_update Qt::Test)
    add_test(NAME updatemanager COMMAND test_updatemanager)

    add_executable(test_spectrumanalyzer
        tests/test_spectrumanalyzer.cpp
        spectrumanalyzer.cpp
        spectrumanalyzer.h
    )
    target_include_directories(test_spectrumanalyzer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(test_spectrumanalyzer PRIVATE Qt::Core Qt::Test)
    set_target_properties(test_spectrumanalyzer PROPERTIES WIN32_EXECUTABLE FALSE)
    add_test(NAME spectrumanalyzer COMMAND test_spectrumanalyzer)

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
        spectrumanalyzer.cpp
        spectrumanalyzer.h
        waveformmodel.cpp
        waveformmodel.h
    )
    target_include_directories(test_waveformwidgets PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(test_waveformwidgets PRIVATE Qt::Core Qt::Widgets Qt::Test app_update)
    set_target_properties(test_waveformwidgets PROPERTIES WIN32_EXECUTABLE FALSE)
    add_test(NAME waveformwidgets COMMAND test_waveformwidgets)

    add_executable(test_mainwindow
        tests/test_mainwindow.cpp
        mainwindow.cpp
        mainwindow.h
        eventlogic.cpp
        eventlogic.h
        spectrumanalyzer.cpp
        spectrumanalyzer.h
        waveformmodel.cpp
        waveformmodel.h
    )
    target_include_directories(test_mainwindow PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(test_mainwindow PRIVATE Qt::Core Qt::Widgets Qt::Test app_update)
    set_target_properties(test_mainwindow PROPERTIES WIN32_EXECUTABLE FALSE)
    add_test(NAME mainwindow COMMAND test_mainwindow)

    set_tests_properties(appversion updatemanager spectrumanalyzer waveformmodel eventlogic waveformwidgets mainwindow PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=minimal"
    )
    if(WIN32)
        find_program(POWERSHELL_EXECUTABLE NAMES pwsh powershell REQUIRED)
        add_test(NAME packaging_contract COMMAND ${POWERSHELL_EXECUTABLE}
            -NoProfile -ExecutionPolicy Bypass -File ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_packaging.ps1)
        get_filename_component(compiler_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
        set_tests_properties(appversion updatemanager spectrumanalyzer waveformmodel eventlogic waveformwidgets mainwindow PROPERTIES
            ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:${compiler_bin};PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
        )
    endif()
endif()
