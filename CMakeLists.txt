cmake_minimum_required(VERSION 2.8)

if (NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    if (DEFINED ENV{VITASDK})
        set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
    else ()
        message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
    endif ()
endif ()

option(USE_VITA_SHARK "Using runtime shader compiler." OFF)
option(ENABLE_EXAMPLE "Enable example." OFF)

project(nanovg-gxm)
include("${VITASDK}/share/vita.cmake" REQUIRED)

set(VITA_APP_NAME ${PROJECT_NAME})
set(VITA_TITLEID "NVG000000")
set(VITA_VERSION "01.00")

# Change this to your PSVita's IP
set(PSVITAIP "192.168.1.140")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -ffast-math -mtune=cortex-a9 -mfpu=neon -Wall")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -ffast-math -mtune=cortex-a9 -mfpu=neon -Wall -std=gnu++17")
set(VITA_MKSFOEX_FLAGS "${VITA_MKSFOEX_FLAGS} -d ATTRIBUTE2=12") # max heap size mode

add_definitions(-D__psp2__ -D__PSV__ -DSTBI_NEON)
if (USE_VITA_SHARK)
    add_definitions(-DUSE_VITA_SHARK)
endif ()

add_subdirectory(src)

if (ENABLE_EXAMPLE)
    add_executable(${PROJECT_NAME}
            ${CMAKE_SOURCE_DIR}/example/perf.c
            ${CMAKE_SOURCE_DIR}/example/demo.c
            ${CMAKE_SOURCE_DIR}/example/example_gxm.c
    )
    target_link_libraries(${PROJECT_NAME}
            nanovg
            SceCtrl_stub
    )

    set(PSV_ASSETS_FILES
            example/sce_sys sce_sys
            example/images example/images
            example/entypo.ttf example/entypo.ttf
            example/Roboto-Regular.ttf example/Roboto-Regular.ttf
            example/Roboto-Bold.ttf example/Roboto-Bold.ttf
            example/NotoEmoji-Regular.ttf example/NotoEmoji-Regular.ttf)
    if (USE_VITA_SHARK)
        list(APPEND PSV_ASSETS_FILES "${CMAKE_BINARY_DIR}/vendor/SceShaccCg" module)
    endif ()

    vita_create_self(${PROJECT_NAME}.self ${PROJECT_NAME} UNSAFE)
    vita_create_vpk(${PROJECT_NAME}.vpk ${VITA_TITLEID} ${PROJECT_NAME}.self
            VERSION ${VITA_VERSION}
            NAME ${VITA_APP_NAME}
            FILE ${PSV_ASSETS_FILES}
    )

    add_custom_target(vpksend
            COMMAND curl -T ${PROJECT_NAME}.vpk ftp://${PSVITAIP}:1337/ux0:/
            DEPENDS ${PROJECT_NAME}.vpk-vpk
    )

    add_custom_target(send
            COMMAND curl -T ${PROJECT_NAME}.self ftp://${PSVITAIP}:1337/ux0:/app/${VITA_TITLEID}/eboot.bin
            DEPENDS ${PROJECT_NAME}.self-self
    )
endif ()
