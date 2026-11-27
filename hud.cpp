#include "shared.h"

// Widescreen HUD (cg_widescreenhud): cgame scales its 640x480 layout to the full screen width, so on
// 16:9 every HUD image is stretched and text is drawn too big (its size follows the width). The
// cgame draw calls (CL_CgameSystemCalls) are routed through here and squeezed into the centered 4:3
// area, positions and text size alike. Images come in pixels, text in 640x480 units (the renderer
// scales it). Menus (ui) and the console are not touched.
// Images that cover the full width (fades, overlays, backgrounds) stay as they are.
// shortcut: names drawn over players' heads are projected to the full width and get pulled toward
// the center as well; correct them in cgame if that turns out to be noticeable.

cvar_t* cg_widescreenhud;

#define RE_FUNC(offset) (*(void**)(0x148BAA0 + (offset))) // client copy of refexport_t
#define VID_WIDTH (*(int*)0x199BCC4)  // renderer glConfig, the size the 2D pass is drawn at
#define VID_HEIGHT (*(int*)0x199BCC8)

static float hudScale, hudCenter;
static int hudWidth, hudHeight;

static bool HUD_Active() {
	float HudScale(int width, int height);
	hudWidth = VID_WIDTH;
	hudHeight = VID_HEIGHT;
	hudScale = cg_widescreenhud && cg_widescreenhud->integer ? HudScale(hudWidth, hudHeight) : 1.0f;
	hudCenter = hudWidth * 0.5f;
	return hudScale < 1.0f;
}

static float HUD_X(float x) { return hudCenter + (x - hudCenter) * hudScale; }
static float HUD_TextX(float x) { return 320.0f + (x - 320.0f) * hudScale; } // text x is in 640 units
static bool HUD_FullWidth(float x, float w) { return x <= 0.5f && x + w >= hudWidth - 0.5f; }
static bool HUD_FullHeight(float y, float h) { return y <= 0.5f && y + h >= hudHeight - 0.5f; }

typedef int(__cdecl* StretchPic_t)(float x, float y, float w, float h, float s1, float t1, float s2, float t2, int shader);
static int __cdecl HUD_StretchPic(float x, float y, float w, float h, float s1, float t1, float s2, float t2, int shader) {
	StretchPic_t draw = (StretchPic_t)RE_FUNC(0x70);
	if (!HUD_Active() || HUD_FullWidth(x, w))
		return draw(x, y, w, h, s1, t1, s2, t2, shader);

	float nx = HUD_X(x), nw = w * hudScale;
	if (HUD_FullHeight(y, h)) { // a full-height piece at a screen edge (e.g. half of a scope): fill the bar with its edge texels
		if (x <= 0.5f) draw(0, y, nx, h, s1, t1, s1, t2, shader);
		if (x + w >= hudWidth - 0.5f) draw(nx + nw, y, hudWidth - (nx + nw), h, s2, t1, s2, t2, shader);
	}
	return draw(nx, y, nw, h, s1, t1, s2, t2, shader);
}

typedef int(__cdecl* StretchPicColor_t)(float x, float y, float w, float h, float s1, float t1, float s2, float t2, int shader, int color, int extra);
static int __cdecl HUD_StretchPicColor(float x, float y, float w, float h, float s1, float t1, float s2, float t2, int shader, int color, int extra) {
	StretchPicColor_t draw = (StretchPicColor_t)RE_FUNC(0x74);
	if (!HUD_Active() || HUD_FullWidth(x, w))
		return draw(x, y, w, h, s1, t1, s2, t2, shader, color, extra);

	float nx = HUD_X(x), nw = w * hudScale;
	if (HUD_FullHeight(y, h)) {
		if (x <= 0.5f) draw(0, y, nx, h, s1, t1, s1, t2, shader, color, extra);
		if (x + w >= hudWidth - 0.5f) draw(nx + nw, y, hudWidth - (nx + nw), h, s2, t1, s2, t2, shader, color, extra);
	}
	return draw(nx, y, nw, h, s1, t1, s2, t2, shader, color, extra);
}

typedef int(__cdecl* RotatedPic_t)(float x, float y, float w, float h, int a5, int a6, int a7, int a8, int a9, int a10);
static int __cdecl HUD_RotatedPic(float x, float y, float w, float h, int a5, int a6, int a7, int a8, int a9, int a10) {
	RotatedPic_t draw = (RotatedPic_t)RE_FUNC(0x78);
	if (HUD_Active() && !HUD_FullWidth(x, w)) {
		x = HUD_X(x);
		w *= hudScale;
	}
	return draw(x, y, w, h, a5, a6, a7, a8, a9, a10);
}

typedef int(__cdecl* QuadPic_t)(const float* verts, const float* st, int shader);
static int __cdecl HUD_QuadPic(const float* verts, const float* st, int shader) { // verts: 4 x,y pairs
	QuadPic_t draw = (QuadPic_t)RE_FUNC(0x7C);
	if (!verts || !HUD_Active())
		return draw(verts, st, shader);
	float v[8];
	for (int i = 0; i < 8; i++)
		v[i] = i & 1 ? verts[i] : HUD_X(verts[i]);
	return draw(v, st, shader);
}

// re.DrawString(x, y, font, scale, color, text, spacing, a8, flags) calls the 11-argument draw as
// (x, y, font, scale, color, text, -1, 0, spacing, a8, flags). spacing: fixed advance per character
// in 640 units (0 = the font's), e.g. 16 for the match timer, so it shrinks with the text.
typedef int(__cdecl* DrawString_t)(float x, float y, int font, float scale, int color, int text, float spacing, int a8, int a9);
static int __cdecl HUD_DrawString(float x, float y, int font, float scale, int color, int text, float spacing, int a8, int a9) {
	DrawString_t draw = (DrawString_t)RE_FUNC(0xD4);
	if (HUD_Active()) {
		x = HUD_TextX(x);
		scale *= hudScale;
		spacing *= hudScale;
	}
	return draw(x, y, font, scale, color, text, spacing, a8, a9);
}

typedef int(__cdecl* DrawStringExt_t)(float x, float y, int font, float scale, int color, int text, int a7, int a8, float spacing, int a10, int a11);
static int __cdecl HUD_DrawStringExt(float x, float y, int font, float scale, int color, int text, int a7, int a8, float spacing, int a10, int a11) {
	DrawStringExt_t draw = (DrawStringExt_t)RE_FUNC(0xE0);
	if (HUD_Active()) {
		x = HUD_TextX(x);
		scale *= hudScale;
		spacing *= hudScale;
	}
	return draw(x, y, font, scale, color, text, a7, a8, spacing, a10, a11);
}

// "call dword ptr [re.X]" (6 bytes) -> "call HUD_X; nop"
static void HUD_PatchCall(DWORD site, void* func) {
	XUNLOCK((void*)site, 6);
	*(BYTE*)site = 0xE8;
	*(int*)(site + 1) = (int)func - (int)(site + 5);
	*(BYTE*)(site + 5) = 0x90;
	FlushInstructionCache(GetCurrentProcess(), (void*)site, 6);
}

void HUD_Init() {
	HUD_PatchCall(0x402EEA, HUD_StretchPic);      // re + 0x70
	HUD_PatchCall(0x402F24, HUD_StretchPicColor); // re + 0x74
	HUD_PatchCall(0x402F5A, HUD_RotatedPic);      // re + 0x78
	HUD_PatchCall(0x402F74, HUD_QuadPic);         // re + 0x7c
	HUD_PatchCall(0x402CEA, HUD_DrawString);      // re + 0xd4
	HUD_PatchCall(0x402D24, HUD_DrawStringExt);   // re + 0xe0
}
