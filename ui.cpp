#include "shared.h"

DWORD uo_ui_mp;

#define UI_FILE_OFF(x) (uo_ui_mp + (x - 0x40000000))

void UI_Init(DWORD base) {
	uo_ui_mp = base;

	PATCH_PUSH_STRING_PTR_VALUE(UI_FILE_OFF(0X4000BD0B), "1.5x");
	PATCH_PUSH_STRING_PTR_VALUE(UI_FILE_OFF(0X4000ED65), "1.5x");
}
