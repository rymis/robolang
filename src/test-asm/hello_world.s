# Simple test: hello world

.text

# r2 = 0
xor r2 r2 r2
load r2 00 08
add r0 r0 r2
:constant
const @hello

load r5 00 08

add r4 r0 r5     # Address of loop

neg r3 r3
out r3
# loop-start:
read8 r3 r2      # Read character
incr r2          # Increment pointer
neg r3 r3        # r3 = ~r3
moveif r0 r4 r3  # if (r3) jump loop

xor r4 r4 r4
stop r4

.data
:hello
"Hello, world!\n"

