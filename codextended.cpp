#include "shared.h"
#include <WinSock2.h>

bool apply_hooks();
bool find_cod_version();

void codextended() {
	srand(time(NULL));

	// All hooks use fixed CoDMP 1.5 addresses. As mss32.dll we are also loaded by CoDSP.exe and the
	// United Offensive exes: there we stay a plain Miles proxy, without patches or messages.
	if (!find_cod_version())
		return;

	if (!apply_hooks()) {
		MsgBox("Failed to initialize CoDExtended, please report this at www.cod1x.eu.\n");
		Com_Quit_f();
	}
}
