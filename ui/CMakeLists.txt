add_library(voidcare_ui STATIC
    include/voidcare/ui/app_controller.h
    include/voidcare/ui/persistence_model.h
    include/voidcare/ui/suspicious_model.h
    src/app_controller.cpp
    src/persistence_model.cpp
    src/suspicious_model.cpp
)

target_include_directories(voidcare_ui
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(voidcare_ui
    PUBLIC
        Qt6::Core
        Qt6::Gui
        voidcare_core
)

target_compile_definitions(voidcare_ui
    PUBLIC
        VOIDCARE_DISCORD_CLIENT_ID="${VOIDCARE_DISCORD_CLIENT_ID}"
)
