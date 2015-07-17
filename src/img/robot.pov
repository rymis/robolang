#include "colors.inc"   // Pre-defined colors
#include "stones.inc"   // Pre-defined scene elements
#include "textures.inc" // Pre-defined textures
#include "shapes.inc"
#include "glass.inc"
#include "metals.inc"
#include "woods.inc"

#include "robot_options.inc"

background { color Black }

#declare TransformLeftArm =
	transform {
		translate <0, 35, 0>
		rotate <LEFT_ARM_ANGLE, 0, 0>
		translate <0, -35, 0>
	}

#declare TransformRightArm =
	transform {
		translate <0, 35, 0>
		rotate <RIGHT_ARM_ANGLE, 0, 0>
		translate <0, -35, 0>
	}

#declare TransformRobot =
	transform {
		rotate <0, BODY_ANGLE, 0>
	}

#declare TransformLeftLeg =
	transform {
		translate <0, 100, 0>
		rotate <LEFT_LEG_ANGLE, 0, 0>
		translate <0, -100, 0>
	}

#declare TransformRightLeg =
	transform {
		translate <0, 100, 0>
		rotate <RIGHT_LEG_ANGLE, 0, 0>
		translate <0, -100, 0>
	}

camera {
	location <0, 80, -120>
	// location <0, -50, -200>
	look_at  <0, -50, 0>
}

// Head:
sphere {
	<0, 0, 0>, 20
	texture {
		pigment {
			P_Silver3
			scale 4
		}

		finish {
			Metal
		}
	}
}

// Eyes:
sphere {
	<10, 3, -15> 3
	texture {
		pigment {
			wood
			scale 0.1
		}
	}
	transform TransformRobot
}
sphere {
	<10, 3, -15>, 5
	texture { Glass2 }
	finish { Glass_Finish }
	transform TransformRobot
}

sphere {
	<-10, 3, -15> 3
	texture {
		pigment {
			wood
			scale 0.1
		}
	}
	transform TransformRobot
}
sphere {
	<-10, 3, -15>, 5
	texture { Glass2 }
	finish { Glass_Finish }
	transform TransformRobot
}

// Body:
union {
	cylinder {
		<0, -15, 0> <0, -25, 0> 8
	}
	box {
		<-20, -24, 10> <20, -100, -10>
	}
	texture {
		pigment {
			P_Silver3
			scale 4
		}

		finish {
			Metal
		}
	}

	transform TransformRobot
}

// Arms:
union {
	// Left arm:
	cylinder {
		<23, -30, 0> <25, -71, 0> 6
	}
	box {
		<30, -70, -3> <20, -75, 3>
	}
	box {
		<30, -75, -3> <28, -85, 3>
	}
	box {
		<22, -75, -3> <20, -85, 3>
	}

	texture {
		pigment {
			P_Silver3
			scale 4
		}

		finish {
			Metal
		}
	}

	transform TransformLeftArm
	transform TransformRobot
}

union {
	// Right arm:
	cylinder {
		<-23, -30, 0> <-25, -71, 0> 6
	}
	box {
		<-30, -70, -3> <-20, -75, 3>
	}
	box {
		<-30, -75, -3> <-28, -85, 3>
	}
	box {
		<-22, -75, -3> <-20, -85, 3>
	}

	texture {
		pigment {
			P_Silver3
			scale 4
		}

		finish {
			Metal
		}
	}

	transform TransformRightArm
	transform TransformRobot
}

// Legs:
union {
	// Left leg:
	cylinder {
		<24, -95, 0> <24, -120, 0> 8
	}
	box {
		<20, -120, 10> <30, -130, -20>
	}

	texture {
		pigment {
			P_Silver3
			scale 4
		}

		finish {
			Metal
		}
	}

	transform TransformLeftLeg
	transform TransformRobot
}

union {
	// Right leg:
	cylinder {
		<-24, -95, 0> <-24, -120, 0> 8
	}
	box {
		<-20, -120, 10> <-30, -130, -20>
	}

	texture {
		pigment {
			P_Silver3
			scale 4
		}

		finish {
			Metal
		}
	}

	transform TransformRightLeg
	transform TransformRobot
}

light_source {
	<100, 100, 100> color White
}
