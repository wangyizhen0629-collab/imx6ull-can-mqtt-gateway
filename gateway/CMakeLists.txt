add_executable(gatewayd
    src/main.c
)

target_compile_features(gatewayd PRIVATE c_std_11)
target_include_directories(gatewayd PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(gatewayd PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
    )
endif()

install(TARGETS gatewayd RUNTIME DESTINATION bin)
install(FILES config/gateway.conf.example
        DESTINATION share/gatewayd
        RENAME gateway.conf.example)

if(BUILD_TESTING)
    add_test(NAME gatewayd_version
             COMMAND gatewayd --version)
    add_test(NAME gatewayd_default_config
             COMMAND gatewayd)
    add_test(NAME gatewayd_example_config
             COMMAND gatewayd --config
                     ${CMAKE_CURRENT_SOURCE_DIR}/config/gateway.conf.example)
endif()

