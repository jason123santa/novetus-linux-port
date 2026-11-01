cmake_minimum_required(VERSION 3.16)
project(novetus-native LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network Xml Qml)
find_package(OpenSSL REQUIRED)

add_library(novcore STATIC
    src/core/paths.cpp
    src/core/crypto.cpp
    src/core/config.cpp
    src/core/clientinfo.cpp
    src/core/script.cpp
    src/core/launcher.cpp
    src/core/discord.cpp
    src/core/linuxcompat.cpp
    src/core/clientsettings.cpp
    src/core/filemanagement.cpp
    src/core/modmanager.cpp
    src/core/upnp.cpp
)
target_link_libraries(novcore PUBLIC Qt6::Core Qt6::Xml Qt6::Network OpenSSL::Crypto)
target_include_directories(novcore PUBLIC src)

add_executable(novetus-native src/cli/main.cpp)
target_link_libraries(novetus-native PRIVATE novcore)

add_executable(novetus-qt src/gui/guishared.cpp src/gui/main.cpp src/gui/mainwindow.cpp src/gui/extendedwindow.cpp src/gui/customizationwindow.cpp src/gui/compactwindow.cpp src/gui/stylishwindow.cpp src/gui/consolewindow.cpp src/gui/serverbrowser.cpp src/gui/settingsdialog.cpp src/gui/graphicsdialog.cpp src/gui/sdkwindow.cpp src/core/webproxy.cpp src/core/extensions.cpp src/gui_assets/assets.qrc)
target_include_directories(novetus-qt PRIVATE src)
target_link_libraries(novetus-qt PRIVATE novcore Qt6::Widgets Qt6::Network Qt6::Qml)
