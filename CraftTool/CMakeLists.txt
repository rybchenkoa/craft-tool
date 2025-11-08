cmake_minimum_required(VERSION 3.12)

project(CraftTool)

#set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# выбираем файлы для сборки
file(GLOB_RECURSE CT_SOURCE ./src/*.cpp)
file(GLOB_RECURSE CT_HEADER ./src/*.h)
file(GLOB_RECURSE CT_RES ./src/*.ui)

# поиск по путям относительно корневой папки
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

# подключаем Qt
find_package(Qt5 COMPONENTS Core Gui OpenGL Widgets REQUIRED)

# разрешаем автозапуск кодогенерации для сигналов и для форм
set(CMAKE_AUTOMOC ON) #signal/slot
set(CMAKE_AUTOUIC ON) #ui
set(CMAKE_AUTORCC ON) #qrc

# готовые exe раскладываем по соответствующим папкам
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/bin/${CMAKE_OUT_BIN}_Debug")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/bin/${CMAKE_OUT_BIN}_Release")

# собираем exe
add_executable(${PROJECT_NAME} ${CT_SOURCE} ${CT_HEADER} ${CT_RES} resources/qt_res.qrc)

# убираем окно консоли
set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE ON)

target_precompile_headers(${PROJECT_NAME} PRIVATE "src/stdafx.h")

target_link_libraries(${PROJECT_NAME} PRIVATE Qt5::Core Qt5::Gui Qt5::OpenGL Qt5::Widgets ws2_32 opengl32)

target_compile_options(${PROJECT_NAME} PRIVATE ${PRESET_COMPILE_OPTIONS})
target_link_options(${PROJECT_NAME} PRIVATE ${PRESET_LINK_OPTIONS})

