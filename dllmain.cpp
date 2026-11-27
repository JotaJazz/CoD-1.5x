#include "stdafx.h"

void codextended();

// PROCESS_DETACH is not called so don't make global declarations which have deconstructors which have to be called.

BOOL APIENTRY DllMain(HMODULE hMod, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
		// load the real mss32.dll first: our exports forward to it even if the hooks below are skipped
		void MSS32_Hook();
		MSS32_Hook();

		extern bool miles32_loaded;
		if (!miles32_loaded) return FALSE;

		codextended();
	}

	return TRUE;
}
