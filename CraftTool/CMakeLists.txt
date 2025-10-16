cmake_minimum_required(VERSION 3.12)

project(CraftTool)

#set(CMAKE_VERBOSE_MAKEFILE ON)
set(CXX_STANDARD 20)
set(CXX_STANDARD_REQUIRED ON)

# выбираем файлы для сборки
file(GLOB_RECURSE CT_SOURCE ./src/*.cpp)
file(GLOB_RECURSE CT_HEADER ./src/*.h)
file(GLOB_RECURSE CT_RES ./src/*.ui)

# раскладываем их в интерфейсе по папкам
#source_group("sources" FILES ${CT_SOURCE})
#source_group("headers" FILES ${CT_HEADER})
#source_group("resources" FILES ${CT_RES})

# поиск по путям относительно корневой папки
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

# подключаем Qt
find_package(Qt5 COMPONENTS Core Gui OpenGL Widgets REQUIRED)

# разрешаем автозапуск кодогенерации для сигналов и для форм
set(CMAKE_AUTOMOC ON) #signal/slot
set(CMAKE_AUTOUIC ON) #ui
set(CMAKE_AUTORCC ON) #qrc

# собираем exe
add_executable(${PROJECT_NAME} ${CT_SOURCE} ${CT_HEADER} ${CT_RES} resources/qt_res.qrc)

# убираем окно консоли
set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE ON)

target_precompile_headers(${PROJECT_NAME} PRIVATE "src/stdafx.h")

target_link_libraries(${PROJECT_NAME} PRIVATE Qt5::Core Qt5::Gui Qt5::OpenGL Qt5::Widgets ws2_32 opengl32)
