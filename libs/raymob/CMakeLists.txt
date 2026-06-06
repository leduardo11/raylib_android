add_library(raymoblib STATIC
    callback.c
    display.c
    helper.c
    sensor.c
    soft_keyboard.c
    vibrator.c
)

target_compile_definitions(raymoblib PRIVATE PLATFORM_ANDROID)

target_include_directories(raymoblib PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${raylib_SOURCE_DIR}
    ${raylib_SOURCE_DIR}/src
    ${ANDROID_NDK}/sources/android/native_app_glue
)

target_link_libraries(raymoblib raylib)
