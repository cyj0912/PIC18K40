cmake_minimum_required(VERSION 3.0)
project(pic18prog)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt5Widgets)
find_package(Qt5SerialPort)

add_definitions(-DUNICODE -D_UNICODE)
file(GLOB SOURCE *.cpp *.h *.rc qhexedit/*.cpp qhexedit/*.h)
add_executable(pic18prog WIN32 ${SOURCE})

target_link_libraries(pic18prog PRIVATE Qt5::Widgets Qt5::SerialPort)
