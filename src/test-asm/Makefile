AS = ../robot_as
LD = ../robot_ld
VM = ../robot_vm

.PHONY: hello_world_test hanoy_test test clean

test: hello_world_test hanoy_test

hello_world_test: hello_world
	$(VM) ./hello_world

hello_world: hello_world.o
	$(LD) -o hello_world hello_world.o

hello_world.o: hello_world.s
	$(AS) hello_world.s

hanoy_test: hanoy
	$(VM) ./hanoy

hanoy: hanoy.o
	$(LD) -o hanoy hanoy.o

hanoy.o: hanoy.s
	$(AS) hanoy.s

clean:
	rm *.o hello_world hanoy

