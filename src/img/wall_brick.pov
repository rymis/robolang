#include "colors.inc"   // Pre-defined colors
#include "stones.inc"   // Pre-defined scene elements
#include "textures.inc" // Pre-defined textures
#include "shapes.inc"
#include "glass.inc"
#include "metals.inc"
#include "woods.inc"

background { color Black }

camera {
	location <0, 200, -300>
	// location <0, -50, -200>
	look_at  <0, -50, 0>
}

// WALLS = array[4]{l, r, u, d}

union {
#if (WALLS[0])
	box {
		<-500, -130, -20> <20, 0, 20>
	}
#end

#if (WALLS[1])
	box {
		<-20, -130, -20> <500, 0, 20>
	}
#end

#if (WALLS[3])
	box {
		<-20, -130, -1000> <20, 0, 20>
	}
#end

#if (WALLS[2])
	box {
		<-20, -130, -20> <20, 0, 1000>
	}
#end

	texture {
		pigment {
			Red_Marble
		}
	}
}


union {
	box {
		<-1000, -140, -1000> <1000, -130, 1000>
	}

	texture {
		PinkAlabaster
	}

}

light_source {
	<100, 100, 100>
	color White
	parallel
	point_at <0, 0, 0>
}

