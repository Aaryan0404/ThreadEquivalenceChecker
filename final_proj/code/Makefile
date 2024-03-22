
# PROGS += state-space.c
# PROGS += 1-kid.c
# PROGS += 2-beginner.c 
# PROGS += 3-amateur.c 
# PROGS += 4-mid.c
# PROGS += 5-cool.c
# PROGS += 6-fast.c
# PROGS += 7-vibes.c
# PROGS += 8-more_vibes.c
PROGS += 9-stack.c

COMMON_SRC += sha256.c
COMMON_SRC += interleaver.c
COMMON_SRC += permutations.c
COMMON_SRC += memory.c
COMMON_SRC += equiv-threads.c
COMMON_SRC += full-except-asm.S
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
	make -C clean
