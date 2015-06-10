# Hanoy tower:

# Go to real_start:
load @real_start
jump

# Data:
:A
{ 41 }
:B
{ 42 }
:C
{ 43 }

:hanoy
# Arguments: a b c n

# if n == 0 return
	load 4  # n
	getnth
	load @hanoy1
	jif
	ret
	:hanoy1

# hanoy(a, c, b, n - 1)
	load 16                         # r n c b a
	getnth
	pushb                           # a r n c b a

	load 12 # now it is c
	getnth
	pushb                           # c a r n c b a

	load 20 # Now it is 
	getnth
	pushb                           # b c a r n c b a

	load 16 # Now it is n
	getnth
	swapab
	decr
	pusha                           # (n-1) b c a r n c b a

	load @hanoy
	call
	popa
	popa
	popa
	popa

# print a '->' c
	load 16 # a
	getnth
	swapab
	out

	# print ' => '
	load 32
	out
	load 61
	out
	load 62
	out
	load 32
	out
	
	load 8 # c
	getnth
	swapab
	out

	load 10
	out

# hanoy(b, a, c, n - 1)
	load 12          # r n c b a
	getnth
	pushb            # b r n c b a

	load 20
	getnth
	pushb            # a b r n c b a

	load 16
	getnth
	pushb            # c a b r n c b a

	load 16
	getnth
	swapab
	decr
	pusha            # (n-1) c a b n c b a

	load @hanoy
	call
	popa
	popa
	popa
	popa

# return
	ret

:hanoy_end

:real_start
	load @A
	r8
	pushb
	load @B
	r8
	pushb
	load @C
	r8
	pushb
	load 3 # n
	pusha

	load @hanoy
	call
	popa
	popa
	popa
	popa

	stop

