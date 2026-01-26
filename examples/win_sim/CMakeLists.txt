cmake_minimum_required(VERSION 3.15)
project(win_sim C)

# 生成 compile_commands.json 用于 IDE 智能提示
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_C_STANDARD 11)

# 编译选项
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra)
endif()

# 调试选项
option(WIN_SIM_ENABLE_DEBUG "Enable debug output" OFF)
if(WIN_SIM_ENABLE_DEBUG)
    add_compile_definitions(SMOTA_ENABLE_DEBUG=1)
endif()

# smOTA 头文件路径
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../smota
    ${CMAKE_CURRENT_SOURCE_DIR}/../../smota/smota_core/inc
)

# 用户配置文件
add_compile_definitions(SMOTA_USER_CONFIG_FILE="smota_user_config.h")

# 源文件
set(SMOTA_CORE_SOURCES
    # 注意：当 smota_core 实现完成后，取消以下注释
    # ${CMAKE_CURRENT_SOURCE_DIR}/../../smota/smota_core/src/smota_core.c
    # ${CMAKE_CURRENT_SOURCE_DIR}/../../smota/smota_core/src/smota_state.c
    # ${CMAKE_CURRENT_SOURCE_DIR}/../../smota/smota_core/src/smota_packet.c
    # ${CMAKE_CURRENT_SOURCE_DIR}/../../smota/smota_core/src/smota_verify.c
)

set(WIN_SIM_SOURCES
    main.c
)

# 创建可执行文件
add_executable(win_sim ${WIN_SIM_SOURCES} ${SMOTA_CORE_SOURCES})