# Simple test: hello world

.text

load r2
const @hello

:loop
read8 r3 r2
load r4
const @end
moveifz r0 r4 r3
out r3
incr r2
load r0
const @loop

:end
xor r4 r4 r4
stop r4

# .data
:hello
"Hello, world!\n"

