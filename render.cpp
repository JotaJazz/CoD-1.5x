#include "shared.h"
#include "client.h"
#include "render.h"
/*
RE_RegisterShaderNoMip_t RE_RegisterShaderNoMip = (RE_RegisterShaderNoMip_t)0x4FCAE0;
RE_RegisterShader_t RE_RegisterShader = (RE_RegisterShader_t)0x4FCA80;
R_FindShader_t R_FindShader = (R_FindShader_t)0x4FC5C0;
SCR_DrawPic_t SCR_DrawPic = (SCR_DrawPic_t)0x4168D0;
SCR_AdjustFrom640_t SCR_AdjustFrom640 = (SCR_AdjustFrom640_t)0x416810;
SCR_DrawSmallStringExt_t SCR_DrawSmallStringExt = (SCR_DrawSmallStringExt_t)0x416A30;
SCR_DrawSmallChar_t SCR_DrawSmallChar = (SCR_DrawSmallChar_t)0x416980;
*/
SCR_DrawString_t SCR_DrawString = (SCR_DrawString_t)0x4E3F00;

RE_SetColor_t RE_SetColor = (RE_SetColor_t)0x4EA700;
/*
RB_SetGL2D_t RB_SetGL2D = (RB_SetGL2D_t)0x4D6CB0;
*/

void R_Init(void) {
	void(*call)(void);
	*(int*)&call = 0x4BF480;
	call();

	if (Cvar_VariableIntegerValue("r_borderless")) {
		int width, height;
		GetDesktopResolution(&width, &height);
		SetWindowLongA(*gameWindow, GWL_EXSTYLE, 0);
		SetWindowLongA(*gameWindow, GWL_STYLE, WS_VISIBLE | WS_POPUP);
		MoveWindow(*gameWindow, 0, 0, width, height, 0);
	}

	void IN_InitRawInput(HWND hWnd);
	IN_InitRawInput(*gameWindow);
}
#include "libs/detours/detours.h"

// The stock vidmode table has no 16:9 modes. Modes 10 (2048x1536, last entry of the video menu)
// and 11 (856x480) become the desktop resolution, and the menu label of mode 10 is renamed to match.
static char nativeModeLabel[32];
static char nativeModeName[2][48];

static const char* __cdecl Localize_Override(const char* key) {
	if (*key == '@') key++;
	return nativeModeLabel[0] && !_stricmp(key, "MENU_20481536") ? nativeModeLabel : NULL;
}

static DWORD orig_SEH_StringEd_GetString;
// engine string lookup: key in edx, one stack arg, returns translation (or NULL) in eax; ecx is scratch
static __declspec(naked) void SEH_StringEd_GetString_Stub() {
	__asm {
		push edx
		push edx
		call Localize_Override
		add esp, 4
		pop edx
		test eax, eax
		jz original
		ret
	original:
		jmp orig_SEH_StringEd_GetString
	}
}

void R_PatchNativeMode() {
	struct vidmode_t { const char* description; int width, height; float pixelAspect; };
	vidmode_t* modes = (vidmode_t*)0x5B21C0;

	int width, height;
	if (!GetDesktopResolution(&width, &height))
		return;

	for (int i = 0; i < 2; i++) {
		int mode = 10 + i;
		_snprintf(nativeModeName[i], sizeof(nativeModeName[i]) - 1, "Mode %d: %dx%d (native)", mode, width, height);
		modes[mode].description = nativeModeName[i];
		modes[mode].width = width;
		modes[mode].height = height;
	}
	_snprintf(nativeModeLabel, sizeof(nativeModeLabel) - 1, "%dx%d", width, height);

	orig_SEH_StringEd_GetString = (DWORD)DetourFunction((PBYTE)0x4B1590, (PBYTE)SEH_StringEd_GetString_Stub);
}
