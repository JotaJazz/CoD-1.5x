#include <math.h>

// Hor+ FOV: fov is the horizontal FOV at 4:3. Wider screens keep the 4:3 vertical FOV and get a
// wider horizontal one; 4:3 and narrower are unchanged. Pure math, tested in tests/unit_test.cpp.
float HorPlusFov(float fov, float aspect) {
	if (aspect <= 4.0f / 3.0f)
		return fov;
	const float toRad = 3.14159265f / 360.0f; // half angle in radians
	return atanf(tanf(fov * toRad) * aspect * 0.75f) / toRad;
}
