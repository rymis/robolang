#!/bin/sh

# Generate wall by vector
wall()
{
cat > "wall_brick_$1.pov" << __EOF__
#declare WALLS = array[4]{$2, $3, $4, $5};

#include "wall_brick.pov"
__EOF__
}

wall h 1 1 0 0
wall v 0 0 1 1
wall lrd 1 1 0 1
wall lru 1 1 1 0
wall lud 1 0 1 1
wall rud 0 1 1 1
wall ld 1 0 0 1
wall lu 1 0 1 0
wall rd 0 1 0 1
wall ru 0 1 1 0
wall x 1 1 1 1

