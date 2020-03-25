cmake_minimum_required(VERSION 3.15)
project(dlvjoy)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)


set(RESOURCES
    ./LaunchScreen.storyboard
    ./gnsh-bitmapfont-colour8.png
)

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

set_property(TARGET SDL2 PROPERTY IMPORTED_LOCATION "${CMAKE_INSTALL_PREFIX}/lib/libSDL2d.a")
set_property(TARGET SDL2main PROPERTY IMPORTED_LOCATION "${CMAKE_INSTALL_PREFIX}/lib/libSDL2maind.a")

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

# set_property(TARGET SDL2 PROPERTY IMPORTED_LOCATION "${CMAKE_INSTALL_PREFIX}/lib/libSDL2-2.0d.dylib")
# set_property(TARGET SDL2main PROPERTY IMPORTED_LOCATION "${CMAKE_INSTALL_PREFIX}/lib/libSDL2maind.a")
# if (IOS)
#     add_custom_command(
#         TARGET dlvjoy
#         POST_BUILD COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different "${CMAKE_INSTALL_PREFIX}/lib/libSDL2-2.0d.dylib" $<TARGET_FILE_DIR:dlvjoy>
#     )
# endif()

target_link_libraries(dlvjoy SDL2 SDL2main)

# target_link_libraries(dlvjoy "$<IF:$<CONFIG:Debug>,SDL2d,SDL2>")
# if (IOS)
#     target_link_libraries(dlvjoy "$<IF:$<CONFIG:Debug>,SDL2maind,SDL2main>")
# endif()

if (IOS)
    set_target_properties(dlvjoy PROPERTIES
        RESOURCE "${RESOURCES}"
        MACOSX_BUNDLE_GUI_IDENTIFIER "software.dll.dlvjoy"
        MACOSX_BUNDLE_BUNDLE_VERSION "1"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "1"
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist
    )
endif()
