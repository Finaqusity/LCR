cmake_minimum_required(VERSION 3.20)
project(lcr-fw C ASM)

# ---------------------------------------------------------------------------
# LCR 电桥测量装置 —— 固件最小工程（裸机，仅 CMSIS，不依赖 HAL）
#   构建: cmake -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake
#         cmake --build build
#   产物: build/lcr-fw.elf / .hex / .bin
# 后面接 HAL 或 CubeMX 生成的工程时，只要把 Core/Src 下的 HAL 源文件加进 SOURCES 即可。
# ---------------------------------------------------------------------------

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(MCU_MODEL STM32F407xx)

set(DRIVERS      ${CMAKE_CURRENT_SOURCE_DIR}/Drivers)
set(CMSIS_DEVICE ${DRIVERS}/CMSIS/Device/ST/STM32F4xx)

set(SOURCES
    Core/Src/main.c
    Core/Src/board.c
    Core/Src/uart.c
    ${CMSIS_DEVICE}/Source/Templates/system_stm32f4xx.c
    ${CMSIS_DEVICE}/Source/Templates/gcc/startup_stm32f407xx.s
)

add_executable(${PROJECT_NAME} ${SOURCES})

# 让 ELF 带 .elf 后缀（默认无扩展名），便于 Cortex-Debug / pyOCD / GDB 指向
set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".elf")

target_include_directories(${PROJECT_NAME} PRIVATE
    Core/Inc
    ${DRIVERS}/CMSIS/Include
    ${CMSIS_DEVICE}/Include
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    ${MCU_MODEL}
    HSE_VALUE=8000000U
)

target_compile_options(${PROJECT_NAME} PRIVATE
    -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard
    -ffunction-sections -fdata-sections
    -Wall -Wextra -Wno-unused-parameter
    $<$<CONFIG:Debug>:-Og -g3>
    $<$<CONFIG:Release>:-O2 -g>
)

target_link_options(${PROJECT_NAME} PRIVATE
    -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard
    -T${CMAKE_CURRENT_SOURCE_DIR}/STM32F407VETx_FLASH.ld
    -Wl,--gc-sections
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.map
    -specs=nano.specs -specs=nosys.specs
    -u_printf_float
)

# 生成 hex / bin，并打印尺寸
# 注意：全部用相对文件名（命令的工作目录就是 build/），不要用 ${CMAKE_BINARY_DIR} 绝对路径。
# 因为仓库/构建目录可能在中文路径下，而 ninja 通过 cmd.exe 调用 objcopy 时，
# 绝对路径里的中文会因代码页不同变成乱码，导致 "No such file or directory"。
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE_NAME:${PROJECT_NAME}> ${PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE_NAME:${PROJECT_NAME}> ${PROJECT_NAME}.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE_NAME:${PROJECT_NAME}>
    COMMENT "生成 hex/bin"
)
