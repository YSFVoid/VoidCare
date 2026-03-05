function(voidcare_add_test target source)
    add_executable(${target} ${source})
    target_link_libraries(${target}
        PRIVATE
            Qt6::Core
            Qt6::Test
            voidcare_core
    )
    add_test(NAME ${target} COMMAND ${target})
endfunction()

voidcare_add_test(voidcare_test_suspicious_scoring unit/test_suspicious_scoring.cpp)
voidcare_add_test(voidcare_test_manifest_roundtrip unit/test_manifest_roundtrip.cpp)
voidcare_add_test(voidcare_test_parser_helpers unit/test_parser_helpers.cpp)
voidcare_add_test(voidcare_test_discord_codec unit/test_discord_codec.cpp)
voidcare_add_test(voidcare_test_process_runner integration/test_process_runner.cpp)
voidcare_add_test(voidcare_test_restore_guard integration/test_restore_guard.cpp)

if(TARGET voidcare_cli_common)
    add_executable(voidcare_test_cli_parser unit/test_cli_parser.cpp)
    target_link_libraries(voidcare_test_cli_parser
        PRIVATE
            Qt6::Core
            Qt6::Test
            voidcare_cli_common
    )
    add_test(NAME voidcare_test_cli_parser COMMAND voidcare_test_cli_parser)

    add_executable(voidcare_test_cli_rendering unit/test_cli_rendering.cpp)
    target_link_libraries(voidcare_test_cli_rendering
        PRIVATE
            Qt6::Core
            Qt6::Test
            voidcare_cli_common
    )
    add_test(NAME voidcare_test_cli_rendering COMMAND voidcare_test_cli_rendering)

    add_executable(voidcare_test_quarantine_index unit/test_quarantine_index.cpp)
    target_link_libraries(voidcare_test_quarantine_index
        PRIVATE
            Qt6::Core
            Qt6::Test
            voidcare_cli_common
    )
    add_test(NAME voidcare_test_quarantine_index COMMAND voidcare_test_quarantine_index)
endif()
