#include "shared.h"
#include <WinSock2.h>

bool apply_hooks();
bool find_cod_version();

void codextended() {
	srand(time(NULL));

	// all hooks use fixed CoDMP 1.5 addresses: never patch any other exe
	if (!find_cod_version()) {
		MsgBox("Failed to find Call of Duty version, please report this at www.cod1x.eu.");
		return;
	}

	if (!apply_hooks()) {
		MsgBox("Failed to initialize CoDExtended, please report this at www.cod1x.eu.\n");
		Com_Quit_f();
	}
}
