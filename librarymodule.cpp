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

// The engine extracts cgame/ui/game DLLs from the highest-priority pk3, which can be one a server
// made us download, and loading a DLL runs its code. Only the official 1.5 DLLs (pakb.pk3) are allowed
// unless cl_allowCustomDlls is set. Hash before loading: a PE header check after loading is too late.
static bool IsAllowedGameDll(const char* path) {
	static const char* official[] = {
		"ba648846f7542bfd4defb82023aae43b4fdebcae5c51f46adedd38e39a5ebaf0", // cgame_mp_x86.dll
		"59e7ae5891a7e08b9a0c715813369dfc50ea33f9b2550a5ca3fc459cdf563c71", // ui_mp_x86.dll
		"b86c24695080007fdb5b595760a2d298bfa17ea0c6fe3e05874c0f19562544f7", // game_mp_x86.dll
	};

	if (Cvar_VariableIntegerValue("cl_allowCustomDlls"))
		return true;

	char hash[65];
	bool Sys_FileSha256(const char* path, char hex[65]);
	if (Sys_FileSha256(path, hash)) {
		for (int i = 0; i < sizeof(official) / sizeof(official[0]); i++)
			if (!strcmp(hash, official[i]))
				return true;
	}

	Com_Printf("^1Blocked %s: not an official Call of Duty 1.5 DLL (it may come from a downloaded pk3).\n"
		"^1Set cl_allowCustomDlls 1 only if you trust the server that sent it.\n", path);
	return false;
}

HMODULE(WINAPI *orig_LoadLibraryA)(LPCSTR lpFileName);
HMODULE WINAPI hLoadLibraryA(LPSTR lpFileName) {
	if (lpFileName && codversion == COD_5 && strstr(lpFileName, "_mp_x86.dll") && !IsAllowedGameDll(lpFileName)) {
		SetLastError(ERROR_ACCESS_DENIED);
		return NULL;
	}

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
