cmake_minimum_required(VERSION 3.10)
SET(VERSION "1.0.0")
project(gobbledegook VERSION "${VERSION}")

#Set C++ standard to C++11
set(CMAKE_CXX_STANDARD 11)

MESSAGE("Detected version: ${VERSION}")

# Add the dependencies
find_package(PkgConfig REQUIRED)
pkg_check_modules(DBUS REQUIRED dbus-1)
pkg_check_modules(GIO REQUIRED gio-2.0)
pkg_check_modules(GLIB REQUIRED glib-2.0)
pkg_check_modules(GOBJECT REQUIRED gobject-2.0)

file(GLOB SOURCE_FILES "src/*.cpp")
file(GLOB INCLUDE_FILES "include/*.h")

SET(GOBBLEDEGOOK_TARGETS ${GOBBLEDEGOOK_TARGETS} gobbledegook)
SET(CMAKE_CONF_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/gobbledegook)

include(CPack)
add_library(gobbledegook SHARED ${SOURCE_FILES})
ADD_LIBRARY(gobbledegook::gobbledegook ALIAS gobbledegook)

INSTALL(TARGETS ${GOBBLEDEGOOK_TARGETS}
    EXPORT gobbledegook-targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

SET(GOBBLEDEGOOK_DEFAULT_LIBRARY gobbledegook)
SET(GOBBLEDEGOOK_TARGETS ${GOBBLEDEGOOK_TARGETS} gobbledegook)
SET_TARGET_PROPERTIES(gobbledegook
    PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS TRUE
    VERSION "${VERSION}")


set_target_properties(gobbledegook PROPERTIES
    FRAMEWORK TRUE
    PUBLIC_HEADER "${INCLUDE_FILES}"
)


export(EXPORT gobbledegook-targets
    FILE "${CMAKE_CURRENT_BINARY_DIR}/gobbledegook-targets.cmake"
    NAMESPACE gobbledegook::)

SET(INCLUDE_INSTALL_DIR include)
include(CMakePackageConfigHelpers)
configure_package_config_file(gobbledegook-config.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/gobbledegook-config.cmake
                              INSTALL_DESTINATION ${CMAKE_CONF_INSTALL_DIR}
                              PATH_VARS INCLUDE_INSTALL_DIR)

INSTALL(EXPORT gobbledegook-targets
        FILE gobbledegook-targets.cmake
        NAMESPACE gobbledegook::
        DESTINATION ${CMAKE_CONF_INSTALL_DIR})

INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/gobbledegook-config.cmake
        DESTINATION ${CMAKE_CONF_INSTALL_DIR})

include_directories(include ${DBUS_INCLUDE_DIRS} ${GIO_INCLUDE_DIRS} ${GLIB_INCLUDE_DIRS} ${GOBJECT_INCLUDE_DIRS})

target_link_libraries(gobbledegook ${DBUS_LIBRARIES} ${GIO_LIBRARIES} ${GLIB_LIBRARIES} ${GOBJECT_LIBRARIES} stdc++fs)

configure_file(${PROJECT_NAME}.pc.in ${PROJECT_NAME}.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)

INSTALL(TARGETS gobbledegook LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})