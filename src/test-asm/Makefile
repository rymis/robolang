AS = ../robot_as
LD = ../robot_ld
VM = ../robot_vm
M4 = m4

TESTS = hello_world hanoy

# Automatically generated...      ##
####################################

TARGETS = $(foreach t,$(TESTS),$(t).test)
EXES = $(patsubst %,%.exe,$(TESTS))

test: $(TARGETS)

%.test: %.exe
	$(VM) $<
	touch $@

%.exe: %.o
	$(LD) -o $@ $<

%.o: %.s
	$(AS) $<

%.s: %.s4
	$(M4) $< > $@

#clean:
#	rm -f *.o $(EXES) $(TARGETS)

