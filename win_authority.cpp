#include "shared.h"

bool find_cod_version() {
	if (!is_addr_safe(0x5A60D0))
		return false;

	if (!strcmp((char*)0x5A60D0, "1.5")) {
		codversion = COD_5;
		return true;
	}
	return false;
}
