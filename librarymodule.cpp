#include "shared.h"
#include "gl/gl.h"
#pragma comment(lib, "libs/detours/detours.lib")
#include "libs/detours/detours.h"

// Our cgame/ui patches use fixed addresses of the official 1.5 DLLs (pakb.pk3). Servers can ship
// other builds, so identify the loaded image by its PE header before touching it.
static bool IsOfficialModule(DWORD base, DWORD timeDateStamp, DWORD sizeOfImage) {
	PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)base;
	PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)(base + dos->e_lfanew);
	return nt->FileHeader.TimeDateStamp == timeDateStamp && nt->OptionalHeader.SizeOfImage == sizeOfImage;
}

HMODULE(WINAPI *orig_LoadLibraryA)(LPCSTR lpFileName);
HMODULE WINAPI hLoadLibraryA(LPSTR lpFileName) {
	HMODULE hModule = orig_LoadLibraryA(lpFileName);
	if (!hModule || codversion != COD_5)
		return hModule;

	DWORD pBase = (DWORD)hModule;
	void Main_UnprotectModule(HMODULE hModule);

	if (strstr(lpFileName, "ui_mp") || strstr(lpFileName, "userinterface")) { /* clients updated by server */
		if (!IsOfficialModule(pBase, 1100126696, 0x226000)) {
			Com_Printf("^3%s is not the official 1.5 build, 1.5x ui patches skipped\n", lpFileName);
			return hModule;
		}
		Main_UnprotectModule(hModule);
		void UI_Init(DWORD);
		UI_Init(pBase);
	}
	else if (strstr(lpFileName, "cgame_mp")) {
		cgame_mp = 0; // CGAME_OFF users check this: never read offsets of an unknown build
		if (!IsOfficialModule(pBase, 1100643646, 0x570000)) {
			Com_Printf("^3%s is not the official 1.5 build, 1.5x cgame patches skipped\n", lpFileName);
			return hModule;
		}
		Main_UnprotectModule(hModule);
		void CG_Init(DWORD);
		CG_Init(pBase);
	}

	return hModule;
}

void patch_opcode_loadlibrary(void) {
	orig_LoadLibraryA = (struct HINSTANCE__ *(__stdcall*)(const char*)) \
	DetourFunction((LPBYTE)LoadLibraryA, (LPBYTE)hLoadLibraryA);
}
