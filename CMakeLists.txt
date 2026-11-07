cmake_minimum_required(VERSION 3.16)
project(dash VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

option(DASH_TEST "Build the dash unit tests" OFF)

set(DASH_PLUGIN_INSTALL_DIR lib/dash/plugins)

# The build tree mirrors the install tree so that the binaries find the
# plugins (bin/../lib/dash/plugins) without being installed.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)

set(CMAKE_INSTALL_RPATH "$ORIGIN/../lib")

find_package(Qt6 6.4 REQUIRED COMPONENTS
    Core Gui Widgets Network DBus Xml Svg
    SerialBus SerialPort WebSockets Bluetooth
    Multimedia MultimediaWidgets Quick QuickWidgets)
find_package(KF6BluezQt REQUIRED)
find_package(openauto 2 REQUIRED)

include(cmake_modules/gitversion.cmake)

# AUTOMOC only finds Q_OBJECT headers it is given; they live apart from the sources.
file(GLOB_RECURSE dash_headers CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/include/*.hpp)

add_library(dash_core SHARED
    ${dash_headers}
    assets/resources.qrc
    src/app/AAHandler.cpp
    src/app/action.cpp
    src/app/arbiter.cpp
    src/app/config.cpp
    src/app/graphics/dynamic_svg.cpp
    src/app/launch_options.cpp
    src/app/pages/launcher.cpp
    src/app/pages/media.cpp
    src/app/pages/openauto.cpp
    src/app/pages/page.cpp
    src/app/pages/settings.cpp
    src/app/pages/vehicle.cpp
    src/app/quick_views/combo.cpp
    src/app/quick_views/quick_view.cpp
    src/app/services/bluetooth.cpp
    src/app/services/clock.cpp
    src/app/services/server.cpp
    src/app/session.cpp
    src/app/stretch_view.cpp
    src/app/utilities/icon_engine.cpp
    src/app/utilities/stylesheet.cpp
    src/app/widgets/climate.cpp
    src/app/widgets/climate_state.cpp
    src/app/widgets/color_picker.cpp
    src/app/widgets/dialog.cpp
    src/app/widgets/fullscreen_toggler.cpp
    src/app/widgets/progress.cpp
    src/app/widgets/selector.cpp
    src/app/widgets/step_meter.cpp
    src/app/widgets/switch.cpp
    src/app/widgets/tuner.cpp
    src/app/widgets/vehicle.cpp
    src/app/widgets/vehicle_state.cpp
    src/app/window.cpp
    src/canbus/elm327.cpp
    src/canbus/socketcanbus.cpp
    src/obd/command.cpp
    src/obd/decoders.cpp
)
target_include_directories(dash_core PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_BINARY_DIR}/generated)
target_compile_options(dash_core PRIVATE -Wall)
target_link_libraries(dash_core PUBLIC
    openauto::openauto
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::DBus Qt6::Xml Qt6::Svg
    Qt6::SerialBus Qt6::SerialPort Qt6::WebSockets Qt6::Bluetooth
    Qt6::Multimedia Qt6::MultimediaWidgets Qt6::Quick Qt6::QuickWidgets
    KF6::BluezQt)
set_target_properties(dash_core PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR})

add_executable(dash src/dash.cpp)
target_link_libraries(dash PRIVATE dash_core)

add_subdirectory(plugins)

install(TARGETS dash_core LIBRARY DESTINATION lib)
install(TARGETS dash RUNTIME DESTINATION bin)

if(DASH_TEST)
    enable_testing()
    add_subdirectory(tests)
endif()
