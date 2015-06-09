# Simple test: hello world

nop

# Go to real_start:
load @real_start
call

# Data string:
:hello
"Hello, world!\n                  "
""""

:real_start
load @hello
pusha

:loop
popa
r8
incr
pusha

# if char is 0 break
load @break
jifnot

swapab
out

load @loop
jump

:break
popa
stop

