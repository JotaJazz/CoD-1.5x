#include <math.h>

// Hor+ FOV: fov is the horizontal FOV at 4:3. Wider screens keep the 4:3 vertical FOV and get a
// wider horizontal one; 4:3 and narrower are unchanged. Pure math, tested in tests/unit_test.cpp.
float HorPlusFov(float fov, float aspect) {
	if (aspect <= 4.0f / 3.0f)
		return fov;
	const float toRad = 3.14159265f / 360.0f; // half angle in radians
	return atanf(tanf(fov * toRad) * aspect * 0.75f) / toRad;
}

// Widescreen HUD: 2D is squeezed horizontally by this factor around the screen center, so the
// 640x480 HUD keeps its 4:3 proportions. 1 at 4:3 and narrower.
float HudScale(int width, int height) {
	return width * 3 > height * 4 ? height * 4.0f / (width * 3.0f) : 1.0f;
}
