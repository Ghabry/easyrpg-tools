cmake_minimum_required(VERSION 3.7)
project(deep8trans VERSION 1.0 LANGUAGES CXX)

# C++14 is required
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

set(CMAKE_MODULE_PATH APPEND "${CMAKE_CURRENT_SOURCE_DIR}/../Modules")

include(ConfigureWindows)

find_package(liblcf REQUIRED)

add_executable(deep8trans
	src/dynrpg.cpp
	src/dynrpg.h
	src/dirent_win.h
	src/entry.cpp
	src/entry.h
	src/main.cpp
	src/translation.cpp
	src/translation.h
	src/types.h
	src/utils.cpp
	src/utils.h
)

target_link_libraries(deep8trans liblcf::liblcf)

include(GNUInstallDirs)
install(TARGETS deep8trans RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
