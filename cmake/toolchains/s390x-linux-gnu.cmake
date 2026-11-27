# Cross-compile for IBM Z (s390x), a big-endian Linux target, and run the
# resulting test binaries under QEMU user-mode emulation.
#
# IFE is little-endian on disk at every version, which is a claim about every
# host. s390x is the big-endian host a Linux runner can reach: the compiler
# runs natively on the build host and only the test binaries are emulated, so
# the build costs what a native build costs.
#
# ctest prefixes every add_test() whose COMMAND is an executable target with
# CMAKE_CROSSCOMPILING_EMULATOR. Tests whose COMMAND is the host Python or
# cmake run natively. The emulator is called directly, which needs no
# binfmt_misc registration on the host: that registration is what failed on
# Blacksmith's runner VMs when the job ran an s390x container instead
# (CI-2).
#
# Host packages (Ubuntu 24.04): g++-s390x-linux-gnu qemu-user
#   g++-s390x-linux-gnu  the cross compiler and the s390x glibc it links
#                        against, installed under /usr/s390x-linux-gnu
#   qemu-user            /usr/bin/qemu-s390x
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR s390x)

set(IFE_CROSS_TRIPLE s390x-linux-gnu)
set(CMAKE_C_COMPILER   ${IFE_CROSS_TRIPLE}-gcc)
set(CMAKE_CXX_COMPILER ${IFE_CROSS_TRIPLE}-g++)

# The dynamic loader and libstdc++ of the target live under the sysroot the
# cross compiler installs. -L tells qemu-s390x to resolve the binary's
# interpreter and shared libraries there instead of under the host's /.
set(CMAKE_CROSSCOMPILING_EMULATOR qemu-s390x -L /usr/${IFE_CROSS_TRIPLE})

# Libraries and headers come from the target sysroot; programs (Python, git,
# the generator) come from the host, since they run at configure time.
set(CMAKE_FIND_ROOT_PATH /usr/${IFE_CROSS_TRIPLE})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
