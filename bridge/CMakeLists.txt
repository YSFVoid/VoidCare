add_executable(VoidCareBridge
    main.cpp
)

target_link_libraries(VoidCareBridge
    PRIVATE
        Qt6::Core
        voidcare_ui
)

target_compile_definitions(VoidCareBridge
    PRIVATE
        VOIDCARE_DISCORD_CLIENT_ID="${VOIDCARE_DISCORD_CLIENT_ID}"
        WIN32_LEAN_AND_MEAN
        NOMINMAX
)

set_target_properties(VoidCareBridge PROPERTIES
    OUTPUT_NAME "VoidCare.Bridge"
)
