cmake_minimum_required(VERSION 3.15)
project(dlvjoy)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

# message(FATAL_ERROR "CMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}")

set(RESOURCES
    "${CMAKE_CURRENT_LIST_DIR}/gnsh-bitmapfont-colour8.png"
    # "${CMAKE_CURRENT_LIST_DIR}/gnsh-bitmapfont-colour9.png"
)
if (IOS)
    list(APPEND RESOURCES
        "${CMAKE_CURRENT_LIST_DIR}/LaunchScreen.storyboard"
    )
# elseif(EMSCRIPTEN)
#     file(GLOB RESOURCES_EMSCRIPTEN
#         "${CMAKE_CURRENT_LIST_DIR}/dlvjoy.*"
#     )
#     list(APPEND RESOURCES
#         ${RESOURCES_EMSCRIPTEN}
#         "${CMAKE_CURRENT_LIST_DIR}/live.js"
#     )
endif()

add_executable(dlvjoy
    main.cpp
    ${RESOURCES}
)

# find_package(SDL2 REQUIRED)
# message("SDL2_INCLUDE_DIRS: (${SDL2_INCLUDE_DIRS})")
# message("SDL2_LIBRARIES: (${SDL2_LIBRARIES})")
# target_include_directories(dlvjoy PRIVATE ${SDL2_INCLUDE_DIRS})
# target_link_libraries(dlvjoy ${SDL2_LIBRARIES})

set_property(TARGET dlvjoy PROPERTY CXX_STANDARD 20)

target_include_directories(dlvjoy PRIVATE "${CMAKE_INSTALL_PREFIX}/include/SDL2")
target_link_directories(dlvjoy PRIVATE "${CMAKE_INSTALL_PREFIX}/lib")

add_library(SDL2 STATIC IMPORTED)
add_library(SDL2main STATIC IMPORTED)
set_property(TARGET SDL2 PROPERTY IMPORTED_LOCATION "${CMAKE_INSTALL_PREFIX}/lib/libSDL2.a")
set_property(TARGET SDL2 PROPERTY IMPORTED_LOCATION_DEBUG "${CMAKE_INSTALL_PREFIX}/lib/libSDL2d.a")
set_property(TARGET SDL2main PROPERTY IMPORTED_LOCATION "${CMAKE_INSTALL_PREFIX}/lib/libSDL2main.a")
set_property(TARGET SDL2main PROPERTY IMPORTED_LOCATION_DEBUG "${CMAKE_INSTALL_PREFIX}/lib/libSDL2maind.a")

target_link_libraries(dlvjoy SDL2)
# if(IOS OR CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    target_link_libraries(dlvjoy SDL2main)
# endif()

# find_package(PkgConfig)
# pkg_check_modules(PC_SDL2 sdl2)

# message("PC_SDL2_INCLUDEDIR: (${PC_SDL2_INCLUDEDIR})")
# message("PC_SDL2_INCLUDE_DIRS: (${PC_SDL2_INCLUDE_DIRS})")
# message("PC_SDL2_LIBDIR: (${PC_SDL2_LIBDIR})")
# message("PC_SDL2_LIBRARY_DIRS: (${PC_SDL2_LIBRARY_DIRS})")


# message("PC_SDL2_FOUND: (${PC_SDL2_FOUND})")
# message("PC_SDL2_LIBRARIES: (${PC_SDL2_LIBRARIES})")
# message("PC_SDL2_LINK_LIBRARIES: (${PC_SDL2_LINK_LIBRARIES})")
# message("PC_SDL2_LIBRARY_DIRS: (${PC_SDL2_LIBRARY_DIRS})")
# message("PC_SDL2_LDFLAGS: (${PC_SDL2_LDFLAGS})")
# message("PC_SDL2_LDFLAGS_OTHER: (${PC_SDL2_LDFLAGS_OTHER})")
# message("PC_SDL2_INCLUDE_DIRS: (${PC_SDL2_INCLUDE_DIRS})")
# message("PC_SDL2_CFLAGS: (${PC_SDL2_CFLAGS})")
# message("PC_SDL2_CFLAGS_OTHER: (${PC_SDL2_CFLAGS_OTHER})")


# find_package(SDL2 REQUIRED)
# message("SDL2_INCLUDE_DIRS: (${SDL2_INCLUDE_DIRS})")
# message("SDL2_LIBRARIES: (${SDL2_LIBRARIES})")
# message("SDL2_FOUND: (${SDL2_FOUND})")
# message("SDL2_VERSION_STRING: (${SDL2_VERSION_STRING})")


# message("SDL2_INCLUDE_DIR: (${SDL2_INCLUDE_DIR})")
# message("SDL2_LIBRARY: (${SDL2_LIBRARY})")
# message("SDL2MAIN_LIBRARY: (${SDL2MAIN_LIBRARY})")

# message(FATAL_ERROR "debug")
# target_include_directories(dlvjoy PRIVATE ${SDL2_INCLUDE_DIRS})
# target_link_libraries(dlvjoy PRIVATE ${SDL2_LIBRARIES})
# target_compile_options(dlvjoy PRIVATE ${PC_SDL2_CFLAGS})
# target_link_options(dlvjoy PRIVATE ${PC_SDL2_LDFLAGS})

if (IOS)
    # if (APPLE)
    #     message(FATAL_ERROR "APPLE is defined")
    # else()
    #     message(FATAL_ERROR "APPLE is not defined")
    # endif()

#     SET_XCODE_PROPERTY(MyTarget CODE_SIGN_IDENTITY "iPhone Developer")
# SET_XCODE_PROPERTY(MyTarget DEVELOPMENT_TEAM ${DEVELOPMENT_TEAM_ID})

    # set(DEVELOPMENT_TEAM_ID "Apple Development: David Ludwig (Q833GXJA54)")
    # set(DEVELOPMENT_TEAM_ID "2791408D9B380240AF0AD46BA6C68CC995606B30")
    set(DEVELOPMENT_TEAM_ID "4XR9U555XB")

    set_target_properties(dlvjoy PROPERTIES
        RESOURCE "${RESOURCES}"
        MACOSX_BUNDLE_GUI_IDENTIFIER "software.dll.dlvjoy"
        MACOSX_BUNDLE_BUNDLE_VERSION "1"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "1"
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist
        XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "iPhone Developer"
        # XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "4XR9U555XB"
        XCODE_ATTRIBUTE_DEVELOPMENT_TEAM ${DEVELOPMENT_TEAM_ID}
        XCODE_ATTRIBUTE_ENABLE_BITCODE "NO"
    )
    target_link_options(dlvjoy PRIVATE "SHELL:-framework AudioToolbox")
    target_link_options(dlvjoy PRIVATE "SHELL:-framework AVFoundation")
    target_link_options(dlvjoy PRIVATE "SHELL:-framework CoreAudio")
    target_link_options(dlvjoy PRIVATE "SHELL:-framework CoreBluetooth")
    target_link_options(dlvjoy PRIVATE "SHELL:-framework CoreGraphics")
    target_link_options(dlvjoy PRIVATE "SHELL:-framework CoreMotion")
    target_link_options(dlvjoy PRIVATE "SHELL:-framework Foundation")
    target_link_options(dlvjoy PRIVATE "SHELL:-framework GameController")
    target_link_options(dlvjoy PRIVATE "SHELL:-framework OpenGLES")
    target_link_options(dlvjoy PRIVATE "SHELL:-framework QuartzCore")
    target_link_options(dlvjoy PRIVATE "SHELL:-framework UIKit")
    target_link_libraries(dlvjoy iconv)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    # set_target_properties(dlvjoy PROPERTIES
    #     RESOURCE "${RESOURCES}"
    #     MACOSX_BUNDLE TRUE
    #     MACOSX_BUNDLE_GUI_IDENTIFIER "software.dll.dlvjoy"
    #     MACOSX_BUNDLE_BUNDLE_VERSION "1"
    #     MACOSX_BUNDLE_SHORT_VERSION_STRING "1"
    #     MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist
    # )
    target_link_options(dlvjoy PRIVATE "SHELL:-framework AppKit")
    target_link_options(dlvjoy PRIVATE "SHELL:-framework AudioToolbox")
    target_link_options(dlvjoy PRIVATE "SHELL:-framework Carbon")
    target_link_options(dlvjoy PRIVATE "SHELL:-framework CoreAudio")
    target_link_options(dlvjoy PRIVATE "SHELL:-framework CoreVideo")
    target_link_options(dlvjoy PRIVATE "SHELL:-framework ForceFeedback")
    target_link_options(dlvjoy PRIVATE "SHELL:-framework Foundation")
    target_link_options(dlvjoy PRIVATE "SHELL:-framework IOKit")
    target_link_libraries(dlvjoy iconv)

    add_custom_command(
        TARGET dlvjoy
        PRE_BUILD COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different "${RESOURCES}" $<TARGET_FILE_DIR:dlvjoy>
    )
elseif (EMSCRIPTEN)
    # target_compile_options(dlvjoy PRIVATE "SHELL:-s TOTAL_MEMORY=8388608")
    set(EMSCRIPTEN_FLAGS "SHELL:-s ASSERTIONS=1 --preload-file gnsh-bitmapfont-colour8.png")  # -s INITIAL_MEMORY=67108864")
    target_compile_options(dlvjoy PRIVATE ${EMSCRIPTEN_FLAGS})
    target_link_options(dlvjoy PRIVATE ${EMSCRIPTEN_FLAGS})
    # target_link_options(dlvjoy PRIVATE "--preload-file gnsh-bitmapfont-colour8.png")
    # foreach(RES ${RESOURCES})
    #     get_filename_component(RES_FILE ${RES} NAME)
    #     message("RES: (${RES})")
    #     message("RES_FILE: (${RES_FILE})")
    #     # target_link_options(dlvjoy PRIVATE "SHELL:--preload-file ${RES_FILE}")
    #     target_compile_options(dlvjoy PRIVATE "SHELL:--preload-file ${RES_FILE}")
    #     target_link_options(dlvjoy PRIVATE "SHELL:--preload-file ${RES_FILE}")
    # endforeach()
    # set(
    # target_compile_options(dlvjoy PRIVATE "--preload-file gnsh-bitmapfont-colour8.png")
    # target_link_options(dlvjoy PRIVATE "--preload-file gnsh-bitmapfont-colour8.png")
    add_custom_command(
        TARGET dlvjoy
        PRE_BUILD COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different "${RESOURCES}" $<TARGET_FILE_DIR:dlvjoy>
    )
    add_custom_command(
        TARGET dlvjoy
        POST_BUILD COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different "${CMAKE_CURRENT_LIST_DIR}/dlvjoy.*" $<TARGET_FILE_DIR:dlvjoy>
    )
    add_custom_command(
        TARGET dlvjoy
        POST_BUILD COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different "${CMAKE_CURRENT_LIST_DIR}/live.js" $<TARGET_FILE_DIR:dlvjoy>
    )
endif()
