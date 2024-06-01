# PROGS += state-space.c
PROGS += set-test.c
# PROGS += 1-basic.c
# PROGS += 1-debug.c
# PROGS += 2-multivar.c 
# PROGS += 3-multifunc.c 
# PROGS += 4-array.c
# PROGS += 5-multi-ctx-switch.c
# PROGS += 6-ldstr-mode.c
# PROGS += 7-vibes.c
# PROGS += 8-more_vibes.c
# PROGS += 9-stack.c
# PROGS += 10-cq.c

COMMON_SRC += interleaver.c
COMMON_SRC += permutations.c
COMMON_SRC += memory.c
COMMON_SRC += equiv-threads.c
COMMON_SRC += full-except-asm.S
COMMON_SRC += switchto-asm.S
COMMON_SRC += mini-step.c
COMMON_SRC += staff-full-except.c 
COMMON_SRC += equiv-rw-set.c

COMMON_SRC += equiv-malloc.c

COMMON_SRC += set.c

COMMON_SRC += equiv-mmu.c
COMMON_SRC += equiv-pt.c
COMMON_SRC += mmu-asm.S

COMMON_SRC += equiv-checker.c

MEMMAP=./memmap

# uncomment if you want it to automatically run.
RUN=1

START_ASM = staff-start.S

LIBS += $(CS140E_2024_PATH)/lib/libgcc.a

BOOTLOADER = pi-install
EXCLUDE ?= grep -v simple_boot
include $(CS140E_2024_PATH)/libpi/mk/Makefile.template-fixed

clean::
	make -C clean
