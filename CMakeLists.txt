cmake_minimum_required(VERSION 3.10)
project(gobbledegook)

#Set C++ standard to C++11
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Add the dependencies
find_package(PkgConfig REQUIRED)
pkg_check_modules(DBUS REQUIRED dbus-1)
pkg_check_modules(GIO REQUIRED gio-2.0)
pkg_check_modules(GLIB REQUIRED glib-2.0)
pkg_check_modules(GOBJECT REQUIRED gobject-2.0)

#Add the source files
file(GLOB SOURCE_FILES "src/*.cpp")

#Add the include directories
include_directories(include ${DBUS_INCLUDE_DIRS} ${GIO_INCLUDE_DIRS} ${GLIB_INCLUDE_DIRS} ${GOBJECT_INCLUDE_DIRS})

#Create the shared library
add_library(gobbledegook SHARED ${SOURCE_FILES})

# Link the dependencies
target_link_libraries(gobbledegook ${DBUS_LIBRARIES} ${GIO_LIBRARIES} ${GLIB_LIBRARIES} ${GOBJECT_LIBRARIES} stdc++fs)

INSTALL(TARGETS gobbledegook DESTINATION lib)