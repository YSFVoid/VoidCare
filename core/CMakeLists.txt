add_library(voidcare_core STATIC
    include/voidcare/core/discord_rpc_service.h
    src/types.cpp
    src/parser_utils.cpp
    src/process_runner.cpp
    src/restore_point_service.cpp
    src/destructive_action_guard.cpp
    src/av_discovery_service.cpp
    src/antivirus_providers.cpp
    src/persistence_audit_service.cpp
    src/suspicious_file_scanner_service.cpp
    src/optimization_service.cpp
    src/gaming_boost_service.cpp
    src/discord_rpc_service.cpp
    src/windows_apps_service.cpp
)

target_include_directories(voidcare_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(voidcare_core
    PUBLIC
        Qt6::Core
        Qt6::Gui
        voidcare_platform_windows
        wbemuuid
        shell32
        psapi
)

target_compile_definitions(voidcare_core
    PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
)
