cmake_minimum_required(VERSION 3.20)
project(lcr-fw C ASM)

# ---------------------------------------------------------------------------
# LCR 自动平衡电桥 RLC 表 —— 固件（裸机，仅 CMSIS 位定义，不依赖 HAL）。
#   构建: cmake -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake
#         cmake --build build
#   产物: build/lcr-fw.elf / .hex / .bin / .map
# 编译纪律（§8.4）：不开 -u_printf_float（newlib-nano 无 %f，浮点输出全走
# results.c 的整数手工排版）；heap=0、禁 malloc/VLA/-ffast-math。
# ---------------------------------------------------------------------------

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(MCU_MODEL STM32F407xx)

set(DRIVERS      ${CMAKE_CURRENT_SOURCE_DIR}/Drivers)
set(CMSIS_DEVICE ${DRIVERS}/CMSIS/Device/ST/STM32F4xx)

# 板上金标准回归（§8.6/§10.2）：默认关，避免拉长 boot 预算；
# test.c 始终参与编译以保证语义正确，仅 boot 调用受 LCR_SELFTEST 门控。
option(LCR_SELFTEST "Run on-board golden-vector regression at boot" OFF)

set(SOURCES
    Core/Src/main.c
    Core/Src/board.c
    Core/Src/uart.c
    Core/Src/dds.c
    Core/Src/relay.c
    Core/Src/adc.c
    Core/Src/dsp.c
    Core/Src/cal.c
    Core/Src/meas.c
    Core/Src/results.c
    Core/Src/i2c_oled.c
    Core/Src/log.c
    Core/Src/selftest.c
    Core/Src/proto.c
    Core/Src/ui.c
    Core/Src/error.c
    Core/Src/syscalls.c
    Core/Src/test.c
    ${CMSIS_DEVICE}/Source/Templates/system_stm32f4xx.c
    ${CMSIS_DEVICE}/Source/Templates/gcc/startup_stm32f407xx.s
)

add_executable(${PROJECT_NAME} ${SOURCES})
set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".elf")

target_include_directories(${PROJECT_NAME} PRIVATE
    Core/Inc
    ${DRIVERS}/CMSIS/Include
    ${CMSIS_DEVICE}/Include
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    ${MCU_MODEL}
    HSE_VALUE=8000000U
    $<$<BOOL:${LCR_SELFTEST}>:LCR_SELFTEST>
)

target_compile_options(${PROJECT_NAME} PRIVATE
    -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard
    -ffunction-sections -fdata-sections -fno-common
    -Wall -Wextra -Wno-unused-parameter
    -Wdouble-promotion -Wshadow -Wformat=2 -Wundef
    $<$<CONFIG:Debug>:-Og -g3>
    $<$<CONFIG:Release>:-O2 -g>
)

target_link_options(${PROJECT_NAME} PRIVATE
    -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard
    -T${CMAKE_CURRENT_SOURCE_DIR}/STM32F407VETx_FLASH.ld
    -Wl,--gc-sections
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.map
    -Wl,--print-memory-usage
    -Wl,--no-warn-rwx-segments
    -specs=nano.specs
)

# libm 必须排在目标文件之后才能解析 cos/sin/atan2/sqrt 等（放 link_options 里
# 会被 ld 先处理而丢弃）。double 数学（校准/自检/金标准）依赖它。
target_link_libraries(${PROJECT_NAME} PRIVATE m)

# 构建期符号/段断言（§8.2，纯 stdlib，任何 Python3 可跑）。
find_package(Python3 QUIET COMPONENTS Interpreter)
if(Python3_EXECUTABLE)
    set(PYTHON_EXE ${Python3_EXECUTABLE})
else()
    set(PYTHON_EXE python3)
endif()
set(CHECK_MAP ${CMAKE_CURRENT_SOURCE_DIR}/../tools/check_map.py)

# 生成 hex/bin、打印尺寸、跑 check_map。全部用相对文件名（工作目录=build/），
# 避免中文路径下 objcopy 经 cmd.exe 的代码页乱码问题。
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE_NAME:${PROJECT_NAME}> ${PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE_NAME:${PROJECT_NAME}> ${PROJECT_NAME}.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE_NAME:${PROJECT_NAME}>
    COMMAND ${PYTHON_EXE} ${CHECK_MAP} ${PROJECT_NAME}.map
    COMMENT "生成 hex/bin + check_map 校验"
)