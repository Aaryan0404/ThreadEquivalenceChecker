
# PROGS += state-space.c
# PROGS += 10-equiv-check.c
# PROGS += 1-test-basic.c
# PROGS += 1-test-two-addr.c
PROGS += 4-test-basic.c
COMMON_SRC += sha256.c
COMMON_SRC += interleaver.c
COMMON_SRC += permutations.c
COMMON_SRC += memory.c
COMMON_SRC += equiv-threads.c
# COMMON_SRC += full-except-asm.S
COMMON_SRC += switchto-asm.S
COMMON_SRC += mini-step.c
COMMON_SRC += staff-full-except.c 

# uncomment if you want it to automatically run.
RUN=1

START_ASM = staff-start.S

LIBS += $(CS140E_2024_PATH)/lib/libgcc.a

BOOTLOADER = my-install
EXCLUDE ?= grep -v simple_boot
include $(CS140E_2024_PATH)/libpi/mk/Makefile.template-fixed

clean::
	make -C tests clean
