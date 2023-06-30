cmake_minimum_required(VERSION 3.0)
project(CAAS_Led_Driver)

find_package(PkgConfig)
find_package(Threads REQUIRED)

#find_package(libconfig++ REQUIRED)
pkg_check_modules(LIBCONFIG++ REQUIRED libconfig++)

add_subdirectory(rpi_ws281x)
add_executable(led_driver
    main.cpp
    LedDriver.h
    LedDriver.cpp
    ArtNet.h
    ArtNet.cpp)
target_link_libraries(led_driver PRIVATE ws2811 ${LIBCONFIG++_LIBRARIES} Threads::Threads)


