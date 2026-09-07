# Toolchain file for arm-none-eabi-gcc
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

# Toolchain executables (assumed to be in PATH, or specify full path)
set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)
set(CMAKE_OBJCOPY arm-none-eabi-objcopy CACHE INTERNAL "objcopy tool")
set(CMAKE_OBJDUMP arm-none-eabi-objdump CACHE INTERNAL "objdump tool")
set(CMAKE_SIZE arm-none-eabi-size CACHE INTERNAL "size tool")

# Prevent CMake from trying to link an executable during compiler testing
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Adjust find root path mode for bare-metal target
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
