cmake_minimum_required(VERSION 3.21)

project(StreamBox VERSION 1.0.0 LANGUAGES C CXX)
include(CTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Multimedia)

set(FFMPEG_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ffmpeg-sdk" CACHE PATH "FFmpeg SDK root")
set(FFMPEG_LIBRARIES avformat avcodec avfilter swresample swscale avutil)
foreach(component IN LISTS FFMPEG_LIBRARIES)
    find_library(FFMPEG_${component}_LIBRARY
        NAMES ${component} lib${component}.dll.a
        PATHS "${FFMPEG_ROOT}/lib"
        NO_DEFAULT_PATH REQUIRED)
endforeach()

qt_add_executable(StreamBox
    src/main.cpp
    src/mainwindow.h
    src/mainwindow.cpp
    src/playerwidgets.h
    src/playerwidgets.cpp
    src/player/playerengine.h
    src/player/playerengine.cpp
)

target_include_directories(StreamBox PRIVATE "${FFMPEG_ROOT}/include")
target_link_libraries(StreamBox PRIVATE
    Qt6::Widgets Qt6::Multimedia
    ${FFMPEG_avformat_LIBRARY}
    ${FFMPEG_avcodec_LIBRARY}
    ${FFMPEG_avfilter_LIBRARY}
    ${FFMPEG_swresample_LIBRARY}
    ${FFMPEG_swscale_LIBRARY}
    ${FFMPEG_avutil_LIBRARY}
)

add_custom_command(TARGET StreamBox POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${FFMPEG_ROOT}/bin" "$<TARGET_FILE_DIR:StreamBox>"
    COMMENT "Deploying FFmpeg runtime libraries")

if(WIN32)
    set_target_properties(StreamBox PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

install(TARGETS StreamBox
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)

if(BUILD_TESTING)
    add_executable(ffmpeg_sdk_smoke scripts/verify_ffmpeg_sdk.c)
    target_include_directories(ffmpeg_sdk_smoke PRIVATE "${FFMPEG_ROOT}/include")
    target_link_libraries(ffmpeg_sdk_smoke PRIVATE
        ${FFMPEG_avformat_LIBRARY} ${FFMPEG_avcodec_LIBRARY}
        ${FFMPEG_avfilter_LIBRARY} ${FFMPEG_swresample_LIBRARY}
        ${FFMPEG_swscale_LIBRARY} ${FFMPEG_avutil_LIBRARY})
    add_test(NAME ffmpeg_sdk_link_and_runtime COMMAND ffmpeg_sdk_smoke)
    set_tests_properties(ffmpeg_sdk_link_and_runtime PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${FFMPEG_ROOT}/bin")
endif()
