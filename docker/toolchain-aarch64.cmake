set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_LIBRARY_ARCHITECTURE aarch64-linux-gnu)

set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)

# Debian multiarch: target libs live in /usr/lib/aarch64-linux-gnu next to the
# host ones, so there is no sysroot to re-root into; CMAKE_LIBRARY_ARCHITECTURE
# keeps find_library away from /usr/lib/x86_64-linux-gnu.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

set(PKG_CONFIG_EXECUTABLE aarch64-linux-gnu-pkg-config CACHE FILEPATH "")

set(QT_HOST_PATH /usr CACHE PATH "")
set(QT_HOST_PATH_CMAKE_DIR /usr/lib/x86_64-linux-gnu/cmake CACHE PATH "")
