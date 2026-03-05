set(VOIDCARE_APP_SOURCES
    main.cpp
    backend_dispatcher.cpp
    ui_state.cpp
    ui_theme.cpp
    ../third_party/imgui/imgui.cpp
    ../third_party/imgui/imgui_draw.cpp
    ../third_party/imgui/imgui_tables.cpp
    ../third_party/imgui/imgui_widgets.cpp
    ../third_party/imgui/backends/imgui_impl_dx11.cpp
    ../third_party/imgui/backends/imgui_impl_win32.cpp
)

add_executable(VoidCare WIN32 ${VOIDCARE_APP_SOURCES})

target_include_directories(VoidCare
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/third_party/imgui
        ${CMAKE_SOURCE_DIR}/third_party/imgui/backends
        ${CMAKE_SOURCE_DIR}/ui/include
)

target_link_libraries(VoidCare
    PRIVATE
        voidcare_ui
        voidcare_core
        voidcare_platform_windows
        Qt6::Core
        Qt6::Gui
        d3d11
        dxgi
        d3dcompiler
        dwmapi
)

target_link_options(VoidCare
    PRIVATE
        "/MANIFESTUAC:level='requireAdministrator' uiAccess='false'"
)

target_compile_definitions(VoidCare
    PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        UNICODE
        _UNICODE
)

set_target_properties(VoidCare PROPERTIES
    OUTPUT_NAME "VoidCare"
    AUTOUIC OFF
    AUTORCC OFF
)
