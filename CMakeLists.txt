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
    src/player/playbackpolicy.h
    src/player/playbackpolicy.cpp
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
    find_package(Qt6 6.5 REQUIRED COMPONENTS Test Network)
    qt_add_executable(playbackpolicy_test
        tests/playbackpolicy_test.cpp
        src/player/playbackpolicy.h
        src/player/playbackpolicy.cpp)
    target_include_directories(playbackpolicy_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(playbackpolicy_test PRIVATE Qt6::Test)
    add_test(NAME playback_policy COMMAND playbackpolicy_test)
    set_tests_properties(playback_policy PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>")

    qt_add_executable(decoderworker_test
        tests/decoderworker_test.cpp
        src/player/playerengine.h
        src/player/playerengine.cpp
        src/player/playbackpolicy.h
        src/player/playbackpolicy.cpp)
    target_include_directories(decoderworker_test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src" "${FFMPEG_ROOT}/include")
    target_link_libraries(decoderworker_test PRIVATE
        Qt6::Test Qt6::Multimedia Qt6::Network
        ${FFMPEG_avformat_LIBRARY} ${FFMPEG_avcodec_LIBRARY}
        ${FFMPEG_avfilter_LIBRARY} ${FFMPEG_swresample_LIBRARY}
        ${FFMPEG_swscale_LIBRARY} ${FFMPEG_avutil_LIBRARY})
    add_test(NAME decoder_worker COMMAND decoderworker_test)
    set_tests_properties(decoder_worker PROPERTIES
        TIMEOUT 25
        ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>\;PATH=path_list_prepend:${FFMPEG_ROOT}/bin")

    qt_add_executable(media_smoke
        tests/media_smoke.cpp
        src/player/playerengine.h
        src/player/playerengine.cpp
        src/player/playbackpolicy.h
        src/player/playbackpolicy.cpp)
    target_include_directories(media_smoke PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src" "${FFMPEG_ROOT}/include")
    target_link_libraries(media_smoke PRIVATE
        Qt6::Multimedia
        ${FFMPEG_avformat_LIBRARY} ${FFMPEG_avcodec_LIBRARY}
        ${FFMPEG_avfilter_LIBRARY} ${FFMPEG_swresample_LIBRARY}
        ${FFMPEG_swscale_LIBRARY} ${FFMPEG_avutil_LIBRARY})

    qt_add_executable(audio_device_test tests/audio_device_test.cpp)
    target_link_libraries(audio_device_test PRIVATE Qt6::Multimedia)

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
