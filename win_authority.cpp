#include "shared.h"

// CoDMP.exe 1.5: PE timestamp of the official build and its version string
bool find_cod_version() {
	PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)GetModuleHandle(NULL);
	PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)((DWORD)dos + dos->e_lfanew);
	if (nt->FileHeader.TimeDateStamp != 1100660134)
		return false;

	if (!is_addr_safe(0x5A60D0) || strcmp((char*)0x5A60D0, "1.5"))
		return false;

	codversion = COD_5;
	return true;
}
