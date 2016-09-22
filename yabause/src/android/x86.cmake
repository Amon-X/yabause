SET(CMAKE_SYSTEM_NAME Linux)
SET(CMAKE_SYSTEM_VERSION 1)

if (NOT DEFINED ENV{ANDROID_NDK_HOME})
	message(FATAL_ERROR "Please set ANDROID_NDK_HOME environment variable")
endif()

set(TOOLCHAIN "$ENV{ANDROID_NDK_HOME}/toolchains/x86-4.9/prebuilt/linux-x86_64/bin/")

SET(CMAKE_C_COMPILER ${TOOLCHAIN}/i686-linux-android-gcc)
SET(CMAKE_CXX_COMPILER ${TOOLCHAIN}/i686-linux-android-g++)
SET(CMAKE_ASM-ATT_COMPILER ${TOOLCHAIN}/i686-linux-android-as)

set(ANDROID_PLATFORM android-24)

set(SYSROOT "$ENV{ANDROID_NDK_HOME}/platforms/${ANDROID_PLATFORM}/arch-x86/usr")

set(CMAKE_C_FLAGS "--sysroot=${SYSROOT}" CACHE STRING "GCC flags" FORCE)
set(CMAKE_CXX_FLAGS "--sysroot=${SYSROOT}" CACHE STRING "G++ flags" FORCE)

set(CMAKE_FIND_ROOT_PATH ${SYSROOT})

SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

SET(ANDROID ON)
SET(ANDROID_ABI x86)
