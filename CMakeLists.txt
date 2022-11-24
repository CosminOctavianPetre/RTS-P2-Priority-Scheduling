cmake_minimum_required(VERSION 3.13)
project(RTS-P2-Priority-Scheduling
        VERSION 0.1.0
        DESCRIPTION "Real time systems Project 2: Priority Scheduling"
        LANGUAGES C
)

# make sure that RTEMS installation path is provided
if(NOT DEFINED RTEMS_INSTALL_PATH)
    message(FATAL_ERROR "RTEMS installation path missing. Please provide it through \"-DRTEMS_INSTALL_PATH=<RTEMS5-INSTALLATION-PATH>\"")
endif()

# set compilation target names
set(TARGET_RTEMS1 music_rtems_part1)
set(TARGET_RTEMS2 music_rtems_part2)

# set language standards
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    set(CMAKE_C_STANDARD 11)
    #set(CMAKE_CXX_STANDARD 20)
endif()

# add RTEMS subdirectory
add_subdirectory(music_rtems)

#TODO
# run pre-build steps (create tarball to be used as read-only FS in RTEMS)
