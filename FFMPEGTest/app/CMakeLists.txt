# Sets the minimum version of CMake required to build the native
# library. You should either keep the default value or only pass a
# value of 3.4.0 or lower.

cmake_minimum_required(VERSION 3.4.1)

set(lib_src_DIR ${CMAKE_SOURCE_DIR}/src/main/jniLibs/${ANDROID_ABI})
include_directories(
        ${CMAKE_SOURCE_DIR}/src/main/cpp/include
)
# Creates and names a library, sets it as either STATIC
# or SHARED, and provides the relative paths to its source code.
# You can define multiple libraries, and CMake builds it for you.
# Gradle automatically packages shared libraries with your APK.

add_library(avcodec-57_lib SHARED IMPORTED)
set_target_properties(avcodec-57_lib PROPERTIES IMPORTED_LOCATION
                             ${lib_src_DIR}/libavcodec-57.so)
add_library(avformat-57_lib SHARED IMPORTED)
set_target_properties(avformat-57_lib PROPERTIES IMPORTED_LOCATION
                      ${lib_src_DIR}/libavformat-57.so)
add_library(avutil-55_lib SHARED IMPORTED)
set_target_properties(avutil-55_lib PROPERTIES IMPORTED_LOCATION
                      ${lib_src_DIR}/libavutil-55.so)
add_library(swresample-2_lib SHARED IMPORTED)
set_target_properties(swresample-2_lib PROPERTIES IMPORTED_LOCATION
                      ${lib_src_DIR}/libswresample-2.so)
add_library(swscale-4_lib SHARED IMPORTED)
set_target_properties(swscale-4_lib PROPERTIES IMPORTED_LOCATION
                      ${lib_src_DIR}/libswscale-4.so)

add_library( # Sets the name of the library.
             native-lib

             # Sets the library as a shared library.
             SHARED

             # Provides a relative path to your source file(s).
             # Associated headers in the same location as their source
             # file are automatically included.
             ${CMAKE_SOURCE_DIR}/src/main/cpp/native-lib.cpp )

# Searches for a specified prebuilt library and stores the path as a
# variable. Because system libraries are included in the search path by
# default, you only need to specify the name of the public NDK library
# you want to add. CMake verifies that the library exists before
# completing its build.

# Specifies libraries CMake should link to your target library. You
# can link multiple libraries, such as libraries you define in the
# build script, prebuilt third-party libraries, or system libraries.

target_link_libraries(native-lib
log
android
avcodec-57_lib
avformat-57_lib
avutil-55_lib
swresample-2_lib
swscale-4_lib)
