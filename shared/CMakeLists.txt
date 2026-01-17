cmake_minimum_required(VERSION 3.29)
project(common VERSION 1.0.0)

add_library(common SHARED
        ${CMAKE_CURRENT_SOURCE_DIR}/cpp/test.cpp)
add_library(common::common ALIAS common)
add_library(common2 SHARED
${CMAKE_CURRENT_SOURCE_DIR}/cpp/test2.cpp)
add_library(common::common2 ALIAS common2)

set(NAME $<TARGET_FILE:common>)
set(TYPE $<$<CONFIG:Debug>:dbg:rel>)
set(TEST $<$<AND:$<CONFIG:DEBUG>,$<STREQUAL:TEST,TEST>>:"TRUE">)
#file(GENERATE OUTPUT common.txt CONTENT
#        "generated value ${NAME} ${TYPE} ${TEST}"
#)

target_include_directories(common PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_include_directories(common2 PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS common common2
        EXPORT commonTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(EXPORT commonTargets
        NAMESPACE common::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/common
)

configure_package_config_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/commonConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/commonConfig.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/common
)
install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/commonConfig.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/common)