find_package(Threads REQUIRED)

# M6 起 gatewayd 使用经过验证的 libmosquitto，而不是手写 MQTT 协议。
# 目标 SDK 若未提供开发文件，应明确配置失败并记录 NOT RUN，不能误用主机库交叉链接。
set(GATEWAY_MOSQUITTO_ROOT "" CACHE PATH
    "Root containing libmosquitto headers and libraries")
find_path(GATEWAY_MOSQUITTO_INCLUDE_DIR
    NAMES mosquitto.h
    HINTS
        "${GATEWAY_MOSQUITTO_ROOT}/include"
        "${GATEWAY_MOSQUITTO_ROOT}/usr/include"
)
find_library(GATEWAY_MOSQUITTO_LIBRARY
    NAMES mosquitto
    HINTS
        "${GATEWAY_MOSQUITTO_ROOT}/lib"
        "${GATEWAY_MOSQUITTO_ROOT}/lib/${CMAKE_LIBRARY_ARCHITECTURE}"
        "${GATEWAY_MOSQUITTO_ROOT}/usr/lib"
        "${GATEWAY_MOSQUITTO_ROOT}/usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}"
)
if(NOT GATEWAY_MOSQUITTO_INCLUDE_DIR OR NOT GATEWAY_MOSQUITTO_LIBRARY)
    message(FATAL_ERROR
        "M6 requires libmosquitto development files. Install them or set "
        "GATEWAY_MOSQUITTO_ROOT to a matching target root.")
endif()
add_library(Mosquitto::Mosquitto UNKNOWN IMPORTED)
set_target_properties(Mosquitto::Mosquitto PROPERTIES
    IMPORTED_LOCATION "${GATEWAY_MOSQUITTO_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${GATEWAY_MOSQUITTO_INCLUDE_DIR}"
)

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
    src/mock_sink.c
    src/mqtt_reactor.c
    src/mqtt_sink.c
    src/pipeline.c
    src/ring_buffer.c
    src/spool.c
    src/stats.c
    src/vehicle_decoder.c
)
target_compile_features(gateway_core PUBLIC c_std_11)
# 生命周期代码使用 POSIX.1-2008；SocketCAN/SO_TIMESTAMPNS 还需要 glibc 的
# Linux/default feature 集。两者同时声明，避免主机与目标 sysroot 可见宏不一致。
target_compile_definitions(gateway_core PUBLIC
    _POSIX_C_SOURCE=200809L
    _DEFAULT_SOURCE
    _FILE_OFFSET_BITS=64
)
target_include_directories(gateway_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(gateway_core PUBLIC Threads::Threads Mosquitto::Mosquitto)

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

    add_gateway_unit_test(test_pipeline M5)
    set_tests_properties(test_pipeline PROPERTIES LABELS "M5;M7;unit")
    add_gateway_unit_test(test_mqtt_sink M6)
    set_tests_properties(test_mqtt_sink PROPERTIES LABELS "M6;M7;M8;unit")
    add_gateway_unit_test(test_mqtt_reactor M8)
    add_gateway_unit_test(test_spool M7)
    add_test(
        NAME test_mqtt_validator
        COMMAND ${GATEWAY_PYTHON3_EXECUTABLE} -B
                ${PROJECT_SOURCE_DIR}/tools/mqtt/test_validate_m6_batches.py
    )
    set_tests_properties(test_mqtt_validator PROPERTIES LABELS "M6;unit")
    add_test(
        NAME test_mqtt_recovery_validator
        COMMAND ${GATEWAY_PYTHON3_EXECUTABLE} -B
                ${PROJECT_SOURCE_DIR}/tools/mqtt/test_validate_m7_recovery.py
    )
    set_tests_properties(test_mqtt_recovery_validator PROPERTIES
                         LABELS "M7;unit")

    # 该可执行文件只在显式启动测试 Broker/subscriber 的 M6 集成 run 中执行，
    # 不注册到默认 CTest，避免普通回归隐式改变 Broker/进程状态。
    add_executable(test_mqtt_integration tests/test_mqtt_integration.c)
    target_compile_features(test_mqtt_integration PRIVATE c_std_11)
    target_link_libraries(test_mqtt_integration PRIVATE gateway_core)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(test_mqtt_integration PRIVATE
                               ${GATEWAY_WARNING_OPTIONS})
    endif()

    add_executable(test_mqtt_timing tests/test_mqtt_timing.c)
    target_compile_features(test_mqtt_timing PRIVATE c_std_11)
    target_link_libraries(test_mqtt_timing PRIVATE gateway_core)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(test_mqtt_timing PRIVATE
                               ${GATEWAY_WARNING_OPTIONS})
    endif()

    # M7 恢复驱动器由显式的 Broker/SIGKILL 脚本调用，不进入默认 CTest。
    add_executable(test_mqtt_recovery_driver
                   tests/test_mqtt_recovery_driver.c)
    target_compile_features(test_mqtt_recovery_driver PRIVATE c_std_11)
    target_link_libraries(test_mqtt_recovery_driver PRIVATE gateway_core)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(test_mqtt_recovery_driver PRIVATE
                               ${GATEWAY_WARNING_OPTIONS})
    endif()
endif()
