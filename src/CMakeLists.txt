set(nanovg_extra_library)

if (USE_VITA_SHARK)
    include(FetchContent)
    FetchContent_Populate(
            libshacccg
            URL "https://codeload.github.com/AnimMouse/SceShaccCg/zip/refs/heads/main"
            SOURCE_DIR "${CMAKE_BINARY_DIR}/vendor/SceShaccCg"
            SUBBUILD_DIR "${CMAKE_BINARY_DIR}/CMakeFiles/SceShaccCg"
            BINARY_DIR "${CMAKE_BINARY_DIR}/CMakeFiles/SceShaccCg"
    )
    list(APPEND nanovg_extra_library
            vitashark
            SceShaccCgExt
            taihen_stub
            SceShaccCg_stub
    )
endif ()

add_library(nanovg STATIC
        ${CMAKE_CURRENT_SOURCE_DIR}/nanovg.c
)
target_include_directories(nanovg PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(nanovg
        m
        SceGxm_stub
        SceCommonDialog_stub
        SceDisplay_stub
        ${nanovg_extra_library}
)
install(TARGETS
        nanovg
        DESTINATION
        ${CMAKE_INSTALL_PREFIX}/lib)
install(FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/nanovg.h
        ${CMAKE_CURRENT_SOURCE_DIR}/nanovg_gxm.h
        ${CMAKE_CURRENT_SOURCE_DIR}/nanovg_gxm_utils.h
        DESTINATION
        ${CMAKE_INSTALL_PREFIX}/include)