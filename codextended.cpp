#include "shared.h"
#include <WinSock2.h>
#include <Shlwapi.h>
#pragma comment(lib, "shlwapi.lib")

bool apply_hooks();
bool find_cod_version();

void codextended() {
	srand(time(NULL));

	// All hooks use fixed CoDMP 1.5 addresses. As mss32.dll we are also loaded by CoDSP.exe and the
	// United Offensive exes: there we stay a plain Miles proxy, without patches or messages.
	if (!find_cod_version()) {
		char exe[MAX_PATH];
		GetModuleFileNameA(NULL, exe, sizeof(exe));
		if (StrStrIA(exe, "codmp")) // looks like CoD MP but not 1.5: say why nothing changed
			MsgBox("CoD 1.5x needs Call of Duty multiplayer 1.5. The game will run without the 1.5x features.");
		return;
	}

	if (!apply_hooks()) {
		MsgBox("Failed to initialize CoDExtended, please report this at www.cod1x.eu.\n");
		Com_Quit_f();
	}
}
