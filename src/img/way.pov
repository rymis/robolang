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

union {
	box {
		<-1000, -140, -1000> <1000, -130, 1000>
	}

	texture {
		PinkAlabaster
	}

}

light_source {
	<100, 100, 100> color White
}

