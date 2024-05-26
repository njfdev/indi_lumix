# define the project name
project(indi-lumix C CXX)
cmake_minimum_required(VERSION 3.22)

include(GNUInstallDirs)

# add our cmake_modules folder
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules")

# find our required packages
find_package(INDI REQUIRED)
find_package(Nova REQUIRED)
find_package(ZLIB REQUIRED)
find_package(GSL REQUIRED)

# these will be used to set the version number in config.h and our driver's xml file
set(INDI_LUMIX_VERSION_MAJOR 0)
set(INDI_LUMIX_VERSION_MINOR 1)

# do the replacement in the config.h
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/config.h
)

# do the replacement in the driver's xml file
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/indi_lumix.xml.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/indi_lumix.xml
)

# set our include directories to look for header files
include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories(SYSTEM ${INDI_INCLUDE_DIR})
include_directories( ${NOVA_INCLUDE_DIR})
include_directories( ${EV_INCLUDE_DIR})

# find libusb
find_path(LIBUSB_INCLUDE_DIR
  NAMES libusb.h
  PATH_SUFFIXES "include" "libusb" "libusb-1.0")
find_library(LIBUSB_LIBRARY
  NAMES usb-1.0
  PATH_SUFFIXES "lib" "lib32" "lib64")

# find liblumix
find_library(LIBLUMIX_LIBRARY
    NAMES lumix
    PATH_SUFFIXES "lib" "lib32" "lib64")

# find pugixml
find_library(PUGIXML_LIBRARY
    NAMES pugixml
    PATH_SUFFIXES "lib" "lib32" "lib64")

include(CMakeCommon)

# tell cmake to build our executable
add_executable(
    indi_lumix
    indi_lumix.cpp
)

# and link it to these libraries
target_link_libraries(
    indi_lumix
    ${INDI_LIBRARIES}
    ${NOVA_LIBRARIES}
    ${GSL_LIBRARIES}
    ${LIBUSB_LIBRARY}
    ${LIBLUMIX_LIBRARY}
    ${PUGIXML_LIBRARY}
    lumix
    usb-1.0
    fmt
    spdlog
)

# and link it to indi dir
target_include_directories(
    indi_lumix
    SYSTEM PUBLIC
    ${INDI_INCLUDE_DIR}
    ${NOVA_INCLUDE_DIR}
    ${GSL_INCLUDE_DIR}
    ${ZLIB_INCLUDE_DIRS}
    ${LIBUSB_INCLUDE_DIR}
    "/usr/local/include"
    "/usr/include/libindi"
)

# tell cmake where to install our executable
install(TARGETS indi_lumix RUNTIME DESTINATION bin)

# and where to put the driver's xml file.
install(
    FILES
    ${CMAKE_CURRENT_BINARY_DIR}/indi_lumix.xml
    DESTINATION ${INDI_DATA_DIR}
)