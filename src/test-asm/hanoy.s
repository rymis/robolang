# Hanoy tower:

.stack 1024
.text

load r0
# Go to real_start:
const @real_start

:A
"A"
:B
"B"
:C
"C"
:TO
" => "

# Function print string:
:print_string
read8 r3 r2
load r4
const @exit_print_string
moveifz r0 r4 r3
out r3
incr r2
load r0
const @print_string
:exit_print_string
# return
read32 r2 r1
incr4 r1
move r0 r2

# Function hanoy (arguments on stack)
:hanoy
move r2 r1 # Stack pointer
read32 r4 r2 # Cnt
const @exit_hanoy
moveifz r0 r2 r4



:exit_hanoy
incr4 r1
incr4 r1
incr4 r1
incr4 r1
read32 r2 r1
incr4 r1
move r0 r2

:real_start


stop r4
