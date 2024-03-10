# change these so you can do a "make test" and it verifies what you already
# have in there.  (probably need a pi in there).   also you will need to 
# do a better design.
# ah: for test, doesn't work for us b/c we don't have the code...
#
# also it is tricky if they swap with our code and their code.
SUBDIRS= #code

.PHONY: all check clean
all check clean: $(SUBDIRS)

all: TARGET=all
check: TARGET=check
clean: TARGET=clean

# No, you can't do TARGET=$@, or at least I don't know how to.

# recursive call to make
$(SUBDIRS): force
	$(MAKE) -s -C $@ $(TARGET)

clean:
	rm -f *~ *.bak

.PHONY: force
	force :;
all: state-space.bin run


state-space.bin:  sha256.c state-space.c libpi.a rpi.h ./Makefile
	$(CC) $(CFLAGS) staff-start.o sha256.c state-space.c -T memmap -o state-space.elf libpi.a
	$(OD) -D  state-space.elf > state-space.list
	$(OCP) state-space.elf -O binary state-space.bin

run: state-space.bin
	@echo "compile was successful: about to run <state-space.bin>"
	pi-install state-space.bin

clean:
	rm -rf *.bin *~ tags *.elf *.list *.d Makefile.bak 

# stop make from deleting these suffix'd files.
.PRECIOUS: %.list %.elf %.o
