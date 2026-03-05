qt_add_executable(VoidCare WIN32
    main.cpp
    voidcare.rc
)

target_link_libraries(VoidCare
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::Widgets
        voidcare_ui
)

set_target_properties(VoidCare PROPERTIES
    OUTPUT_NAME "VoidCare"
    QT_QML_MODULE_NO_IMPORT_SCAN TRUE
)
