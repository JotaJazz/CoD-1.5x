#include "shared.h"

// Widescreen HUD (cg_widescreenhud): cgame scales its 640x480 layout to the full screen width, so on
// 16:9 every HUD image is stretched and text is drawn too big (its size follows the width). The
// cgame draw calls (CL_CgameSystemCalls) are routed through here and squeezed into the centered 4:3
// area, positions and text size alike. Images come in pixels, text in 640x480 units (the renderer
// scales it). The menus (ui_mp, CL_UISystemCalls) get the same with ui_widescreen; the ui keeps its
// cursor in 640x480 units and draws it through here too, so clicks stay on what is shown.
// The console is not touched.
// Images that cover the full width (fades, overlays, backgrounds) stay as they are.
// shortcut: names drawn over players' heads are projected to the full width and get pulled toward
// the center as well; correct them in cgame if that turns out to be noticeable.

cvar_t* cg_widescreenhud;
cvar_t* ui_widescreen;

#define RE_FUNC(offset) (*(void**)(0x148BAA0 + (offset))) // client copy of refexport_t
#define VID_WIDTH (*(int*)0x199BCC4)  // renderer glConfig, the size the 2D pass is drawn at
#define VID_HEIGHT (*(int*)0x199BCC8)

static float hudScale, hudCenter;
static int hudWidth, hudHeight;

static bool HUD_Active(cvar_t* enabled) {
	float HudScale(int width, int height);
	hudWidth = VID_WIDTH;
	hudHeight = VID_HEIGHT;
	hudScale = enabled && enabled->integer ? HudScale(hudWidth, hudHeight) : 1.0f;
	hudCenter = hudWidth * 0.5f;
	return hudScale < 1.0f;
}

static float HUD_X(float x) { return hudCenter + (x - hudCenter) * hudScale; }
static float HUD_TextX(float x) { return 320.0f + (x - 320.0f) * hudScale; } // text x is in 640 units
static bool HUD_FullWidth(float x, float w) { return x <= 0.5f && x + w >= hudWidth - 0.5f; }
static bool HUD_FullHeight(float y, float h) { return y <= 0.5f && y + h >= hudHeight - 0.5f; }

typedef int(__cdecl* StretchPic_t)(float x, float y, float w, float h, float s1, float t1, float s2, float t2, int shader);
template <cvar_t** enabled> static int __cdecl HUD_StretchPic(float x, float y, float w, float h, float s1, float t1, float s2, float t2, int shader) {
	StretchPic_t draw = (StretchPic_t)RE_FUNC(0x70);
	if (!HUD_Active(*enabled) || HUD_FullWidth(x, w))
		return draw(x, y, w, h, s1, t1, s2, t2, shader);

	float nx = HUD_X(x), nw = w * hudScale;
	if (HUD_FullHeight(y, h)) { // a full-height piece at a screen edge (e.g. half of a scope): fill the bar with its edge texels
		if (x <= 0.5f) draw(0, y, nx, h, s1, t1, s1, t2, shader);
		if (x + w >= hudWidth - 0.5f) draw(nx + nw, y, hudWidth - (nx + nw), h, s2, t1, s2, t2, shader);
	}
	return draw(nx, y, nw, h, s1, t1, s2, t2, shader);
}

typedef int(__cdecl* StretchPicColor_t)(float x, float y, float w, float h, float s1, float t1, float s2, float t2, int shader, int color, int extra);
template <cvar_t** enabled> static int __cdecl HUD_StretchPicColor(float x, float y, float w, float h, float s1, float t1, float s2, float t2, int shader, int color, int extra) {
	StretchPicColor_t draw = (StretchPicColor_t)RE_FUNC(0x74);
	if (!HUD_Active(*enabled) || HUD_FullWidth(x, w))
		return draw(x, y, w, h, s1, t1, s2, t2, shader, color, extra);

	float nx = HUD_X(x), nw = w * hudScale;
	if (HUD_FullHeight(y, h)) {
		if (x <= 0.5f) draw(0, y, nx, h, s1, t1, s1, t2, shader, color, extra);
		if (x + w >= hudWidth - 0.5f) draw(nx + nw, y, hudWidth - (nx + nw), h, s2, t1, s2, t2, shader, color, extra);
	}
	return draw(nx, y, nw, h, s1, t1, s2, t2, shader, color, extra);
}

typedef int(__cdecl* RotatedPic_t)(float x, float y, float w, float h, int a5, int a6, int a7, int a8, int a9, int a10);
template <cvar_t** enabled> static int __cdecl HUD_RotatedPic(float x, float y, float w, float h, int a5, int a6, int a7, int a8, int a9, int a10) {
	RotatedPic_t draw = (RotatedPic_t)RE_FUNC(0x78);
	if (HUD_Active(*enabled) && !HUD_FullWidth(x, w)) {
		x = HUD_X(x);
		w *= hudScale;
	}
	return draw(x, y, w, h, a5, a6, a7, a8, a9, a10);
}

typedef int(__cdecl* QuadPic_t)(const float* verts, const float* st, int shader);
template <cvar_t** enabled> static int __cdecl HUD_QuadPic(const float* verts, const float* st, int shader) { // verts: 4 x,y pairs
	QuadPic_t draw = (QuadPic_t)RE_FUNC(0x7C);
	if (!verts || !HUD_Active(*enabled))
		return draw(verts, st, shader);
	float v[8];
	for (int i = 0; i < 8; i++)
		v[i] = i & 1 ? verts[i] : HUD_X(verts[i]);
	return draw(v, st, shader);
}

// The menus show the version in the bottom-right corner (menu_background.menu: cvar "shortversion",
// right edge at x 630). When it is drawn, the slogan goes in the bottom-left corner, mirrored.
#define MENU_VERSION_RIGHT 630
#define MENU_SLOGAN "CoD still rocks!"

static bool UI_IsVersionText(const char* text) {
	static cvar_t* shortversion;
	if (!shortversion)
		shortversion = Cvar_Get("shortversion", "", 0); // registered by the engine at startup, value kept
	return text && shortversion->string && *shortversion->string && !strcmp(text, shortversion->string);
}

// re.DrawString(x, y, font, scale, color, text, spacing, a8, flags) calls the 11-argument draw as
// (x, y, font, scale, color, text, -1, 0, spacing, a8, flags). spacing: fixed advance per character
// in 640 units (0 = the font's), e.g. 16 for the match timer, so it shrinks with the text.
typedef int(__cdecl* DrawString_t)(float x, float y, int font, float scale, int color, int text, float spacing, int a8, int a9);
template <cvar_t** enabled> static int __cdecl HUD_DrawString(float x, float y, int font, float scale, int color, int text, float spacing, int a8, int a9) {
	DrawString_t draw = (DrawString_t)RE_FUNC(0xD4);
	float slogan = 640 - MENU_VERSION_RIGHT; // mirror of the version's right edge
	if (HUD_Active(*enabled)) {
		x = HUD_TextX(x);
		slogan = HUD_TextX(slogan);
		scale *= hudScale;
		spacing *= hudScale;
	}
	if (enabled == &ui_widescreen && UI_IsVersionText((const char*)text))
		draw(slogan, y, font, scale, color, (int)MENU_SLOGAN, spacing, a8, a9);
	return draw(x, y, font, scale, color, text, spacing, a8, a9);
}

typedef int(__cdecl* DrawStringExt_t)(float x, float y, int font, float scale, int color, int text, int a7, int a8, float spacing, int a10, int a11);
template <cvar_t** enabled> static int __cdecl HUD_DrawStringExt(float x, float y, int font, float scale, int color, int text, int a7, int a8, float spacing, int a10, int a11) {
	DrawStringExt_t draw = (DrawStringExt_t)RE_FUNC(0xE0);
	float slogan = 640 - MENU_VERSION_RIGHT;
	if (HUD_Active(*enabled)) {
		x = HUD_TextX(x);
		slogan = HUD_TextX(slogan);
		scale *= hudScale;
		spacing *= hudScale;
	}
	if (enabled == &ui_widescreen && UI_IsVersionText((const char*)text))
		draw(slogan, y, font, scale, color, (int)MENU_SLOGAN, a7, a8, spacing, a10, a11);
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
	// CL_CgameSystemCalls
	HUD_PatchCall(0x402EEA, HUD_StretchPic<&cg_widescreenhud>);      // re + 0x70
	HUD_PatchCall(0x402F24, HUD_StretchPicColor<&cg_widescreenhud>); // re + 0x74
	HUD_PatchCall(0x402F5A, HUD_RotatedPic<&cg_widescreenhud>);      // re + 0x78
	HUD_PatchCall(0x402F74, HUD_QuadPic<&cg_widescreenhud>);         // re + 0x7c
	HUD_PatchCall(0x402CEA, HUD_DrawString<&cg_widescreenhud>);      // re + 0xd4
	HUD_PatchCall(0x402D24, HUD_DrawStringExt<&cg_widescreenhud>);   // re + 0xe0
	HUD_PatchCall(0x40A88D, HUD_DrawString<&cg_widescreenhud>);      // chat lines, drawn by the exe for cgame

	// CL_UISystemCalls
	HUD_PatchCall(0x41A4DF, HUD_StretchPic<&ui_widescreen>);
	HUD_PatchCall(0x41A8AB, HUD_DrawString<&ui_widescreen>);
	HUD_PatchCall(0x41A8E7, HUD_DrawStringExt<&ui_widescreen>);
}
