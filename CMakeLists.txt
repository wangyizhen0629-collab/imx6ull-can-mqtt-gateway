cmake_minimum_required(VERSION 3.16)

project(imx6ull_can_mqtt_gateway VERSION 0.1.0 LANGUAGES C)

option(GATEWAY_ENABLE_SANITIZERS
       "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

if(GATEWAY_ENABLE_SANITIZERS)
    if(NOT CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        message(FATAL_ERROR "Sanitizers require GCC or Clang")
    endif()
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

include(CTest)
add_subdirectory(gateway)
