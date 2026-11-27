#include "shared.h"

// CoDMP.exe 1.5 (retail or Steam): version string at a fixed .rdata address. CoDSP.exe and the
// United Offensive exes, which also load us as mss32.dll, don't have it there.
bool find_cod_version() {
	if (!is_addr_safe(0x5A60D0) || strcmp((char*)0x5A60D0, "1.5"))
		return false;

	codversion = COD_5;
	return true;
}
