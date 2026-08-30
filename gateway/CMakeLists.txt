find_package(Threads REQUIRED)

set(GATEWAY_WARNING_OPTIONS
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

add_library(gateway_core STATIC
    src/can_receiver.c
    src/config.c
    src/error.c
    src/lifecycle.c
    src/log.c
    src/ring_buffer.c
    src/stats.c
    src/vehicle_decoder.c
)
target_compile_features(gateway_core PUBLIC c_std_11)
# 生命周期代码使用 POSIX.1-2008；SocketCAN/SO_TIMESTAMPNS 还需要 glibc 的
# Linux/default feature 集。两者同时声明，避免主机与目标 sysroot 可见宏不一致。
target_compile_definitions(gateway_core PUBLIC
    _POSIX_C_SOURCE=200809L
    _DEFAULT_SOURCE
)
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

    function(add_gateway_unit_test test_name milestone_label)
        add_executable(${test_name} tests/${test_name}.c)
        target_compile_features(${test_name} PRIVATE c_std_11)
        target_link_libraries(${test_name} PRIVATE gateway_core)
        if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(${test_name} PRIVATE ${GATEWAY_WARNING_OPTIONS})
        endif()
        add_test(NAME ${test_name} COMMAND ${test_name})
        set_tests_properties(${test_name} PROPERTIES
                             LABELS "${milestone_label};unit")
    endfunction()

    add_gateway_unit_test(test_config M1)
    add_gateway_unit_test(test_error_record M1)
    add_gateway_unit_test(test_lifecycle M1)
    add_gateway_unit_test(test_log M1)
    add_gateway_unit_test(test_ring_buffer M1)
    add_gateway_unit_test(test_can_receiver M2)
    add_gateway_unit_test(test_vehicle_decoder M4)
    target_compile_definitions(test_vehicle_decoder PRIVATE
        "GATEWAY_GOLDEN_VECTOR_PATH=\"${PROJECT_SOURCE_DIR}/protocol/test_vectors/vehicle_golden.csv\""
    )

    find_program(GATEWAY_PYTHON3_EXECUTABLE NAMES python3 REQUIRED)
    add_test(
        NAME test_vehicle_dbc
        COMMAND ${GATEWAY_PYTHON3_EXECUTABLE}
                ${PROJECT_SOURCE_DIR}/tools/protocol/check_vehicle_dbc.py
                --dbc ${PROJECT_SOURCE_DIR}/protocol/vehicle.dbc
                --vectors
                ${PROJECT_SOURCE_DIR}/protocol/test_vectors/vehicle_golden.csv
    )
    set_tests_properties(test_vehicle_dbc PROPERTIES LABELS "M4;protocol")
endif()
