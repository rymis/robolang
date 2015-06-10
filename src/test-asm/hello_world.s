# Simple test: hello world

# Go to real_start:
load @real_start
jump

# Data string:
:hello
"Hello, world!\n"

:real_start
load @hello  # A = &hello
pusha        # PUSH(A)

:loop
popa         # A = TOP()
r8           # B = *A
incr         # ++A
pusha        # PUSH(A)

# if char is 0 break
load @break  # A = &break
jifnot       # if (!B) jump

swapab       # A, B = B, A
out          # putchar(A)

load @loop   # A = &loop
jump         # jump

:break
popa         # Stack is in original state
load 0       # A = 0
stop         # exit(A)

