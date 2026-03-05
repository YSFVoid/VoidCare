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
