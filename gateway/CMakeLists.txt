find_package(Threads REQUIRED)

set(GATEWAY_WARNING_OPTIONS
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

add_library(gateway_core STATIC
    src/config.c
    src/error.c
    src/lifecycle.c
    src/log.c
    src/ring_buffer.c
    src/stats.c
)
target_compile_features(gateway_core PUBLIC c_std_11)
target_compile_definitions(gateway_core PUBLIC _POSIX_C_SOURCE=200809L)
target_include_directories(gateway_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(gateway_core PUBLIC Threads::Threads)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(gateway_core PRIVATE ${GATEWAY_WARNING_OPTIONS})
endif()

add_executable(gatewayd src/main.c)
target_compile_features(gatewayd PRIVATE c_std_11)
target_link_libraries(gatewayd PRIVATE gateway_core)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(gatewayd PRIVATE ${GATEWAY_WARNING_OPTIONS})
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

    function(add_gateway_unit_test test_name)
        add_executable(${test_name} tests/${test_name}.c)
        target_compile_features(${test_name} PRIVATE c_std_11)
        target_link_libraries(${test_name} PRIVATE gateway_core)
        if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(${test_name} PRIVATE ${GATEWAY_WARNING_OPTIONS})
        endif()
        add_test(NAME ${test_name} COMMAND ${test_name})
        set_tests_properties(${test_name} PROPERTIES LABELS "M1;unit")
    endfunction()

    add_gateway_unit_test(test_config)
    add_gateway_unit_test(test_error_record)
    add_gateway_unit_test(test_lifecycle)
    add_gateway_unit_test(test_log)
    add_gateway_unit_test(test_ring_buffer)
endif()
