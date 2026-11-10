# arm-none-eabi 交叉编译工具链（供 cmake -DCMAKE_TOOLCHAIN_FILE=... 使用）
set(CMAKE_SYSTEM_NAME      Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_C_COMPILER   arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)
set(CMAKE_OBJCOPY      arm-none-eabi-objcopy)
set(CMAKE_OBJDUMP      arm-none-eabi-objdump)
set(CMAKE_SIZE         arm-none-eabi-size)
set(CMAKE_AR           arm-none-eabi-ar)

# 只在工具链目录里找程序，避免混入主机工具
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# 关键：裸机工程没有 syscalls（_write/_sbrk 等），CMake 默认会链接一个可执行文件来
# 测试编译器是否能工作，这一步必然失败。改为只编译静态库，跳过链接测试。
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

