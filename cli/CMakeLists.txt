add_library(voidcare_cli_common STATIC
    cli_common.cpp
    cli_common.h
)

target_include_directories(voidcare_cli_common
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(voidcare_cli_common
    PUBLIC
        Qt6::Core
        voidcare_core
)

add_executable(voidcare_cli
    voidcare_main.cpp
)

target_link_libraries(voidcare_cli
    PRIVATE
        Qt6::Core
        Qt6::Gui
        voidcare_core
        voidcare_platform_windows
        voidcare_cli_common
)

target_compile_definitions(voidcare_cli
    PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
)

set_target_properties(voidcare_cli PROPERTIES
    OUTPUT_NAME "voidcare"
)

