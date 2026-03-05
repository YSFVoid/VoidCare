add_library(voidcare_platform_windows STATIC
    src/admin_utils.cpp
    src/signature_verifier.cpp
    src/hash_utils.cpp
    src/windows_paths.cpp
)

target_include_directories(voidcare_platform_windows
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(voidcare_platform_windows
    PUBLIC
        Qt6::Core
        wintrust
        crypt32
        bcrypt
        version
)

target_compile_definitions(voidcare_platform_windows
    PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
)
