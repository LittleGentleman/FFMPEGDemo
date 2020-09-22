# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.

cmake_minimum_required(VERSION 3.4.1)
#这个目录下的源文件添加到库中
aux_source_directory(src/main/cpp SOURCE)

# Creates and names a library, sets it as either STATIC
# or SHARED, and provides the relative paths to its source code.
# You can define multiple libraries, and CMake builds them for you.
# Gradle automatically packages shared libraries with your APK.

add_library( # Sets the name of the library.
             native-lib

             # Sets the library as a shared library.
             SHARED

             # Provides a relative path to your source file(s).
             #src/main/cpp/native-lib.cpp
            ${SOURCE})


#添加库 方式一：
#添加第三方的静态库和动态库  SHARED是动态库，STATIC是静态库  IMPORTED表示这个库是导入的
#add_library(avcodec STATIC IMPORTED)
#设置这个第三方库的路径
#set_target_properties(avcodec PROPERTIES IMPORTED_LOCATION src/main/cpp/include/)

#添加库 方式二：
#设置一个变量
set(libs ${CMAKE_SOURCE_DIR}/src/main/cpp/${ANDROID_ABI})
#-L:引入库查找路径
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -L${libs}")

#引入头文件
include_directories(src/main/cpp/include)


# Searches for a specified prebuilt library and stores the path as a
# variable. Because CMake includes system libraries in the search path by
# default, you only need to specify the name of the public NDK library
# you want to add. CMake verifies that the library exists before
# completing its build.

#添加系统提供的库
find_library( # Sets the name of the path variable.
              log-lib

              # Specifies the name of the NDK library that
              # you want CMake to locate.
              log )

# Specifies libraries CMake should link to your target library. You
# can link multiple libraries, such as libraries you define in this
# build script, prebuilt third-party libraries, or system libraries.

target_link_libraries( # Specifies the target library.
                       native-lib
        avfilter avformat avcodec avutil swresample swscale rtmp z android
        OpenSLES

                       # Links the target library to the log library
                       # included in the NDK.
                       ${log-lib} )