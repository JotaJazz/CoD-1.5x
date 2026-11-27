#include <windows.h>
#include <dbghelp.h>
#include <intrin.h>
#include <stdio.h>
#include <stdlib.h>
#include "version.h"
#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")

// Crash handler: on an unhandled exception, save crashes\crash-<time>.txt (summary, registers, likely
// return addresses) and a small .dmp next to the exe, then tell the player where they are.
// Win32 only, no game calls, so tests/crash_test.cpp can exercise it. Nothing is sent anywhere.

static const char* ExceptionName(DWORD code) {
	switch (code) {
	case EXCEPTION_ACCESS_VIOLATION: return "ACCESS_VIOLATION";
	case EXCEPTION_STACK_OVERFLOW: return "STACK_OVERFLOW";
	case EXCEPTION_ILLEGAL_INSTRUCTION: return "ILLEGAL_INSTRUCTION";
	case EXCEPTION_PRIV_INSTRUCTION: return "PRIV_INSTRUCTION";
	case EXCEPTION_INT_DIVIDE_BY_ZERO: return "INT_DIVIDE_BY_ZERO";
	case EXCEPTION_FLT_DIVIDE_BY_ZERO: return "FLT_DIVIDE_BY_ZERO";
	case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "ARRAY_BOUNDS_EXCEEDED";
	case EXCEPTION_IN_PAGE_ERROR: return "IN_PAGE_ERROR";
	default: return "UNKNOWN";
	}
}

// "module.dll+0x1234" for an address inside a loaded module, else the raw address
static void DescribeAddress(DWORD addr, char* out, int size) {
	HMODULE mod = NULL;
	char path[MAX_PATH];
	if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)addr, &mod)
		&& GetModuleFileNameA(mod, path, sizeof(path))) {
		const char* name = strrchr(path, '\\');
		_snprintf(out, size - 1, "%s+0x%X", name ? name + 1 : path, addr - (DWORD)mod);
	}
	else _snprintf(out, size - 1, "0x%08X", addr);
	out[size - 1] = 0;
}

static BOOL CALLBACK MinimizeOwnWindow(HWND hWnd, LPARAM) {
	DWORD pid;
	GetWindowThreadProcessId(hWnd, &pid);
	if (pid == GetCurrentProcessId() && IsWindowVisible(hWnd))
		ShowWindow(hWnd, SW_MINIMIZE);
	return TRUE;
}

static LONG WINAPI Crash_Filter(EXCEPTION_POINTERS* ep) {
	static LONG entered;
	if (InterlockedExchange(&entered, 1)) // crashed while reporting: let Windows handle it
		return EXCEPTION_CONTINUE_SEARCH;

	EXCEPTION_RECORD* er = ep->ExceptionRecord;
	CONTEXT* c = ep->ContextRecord;

	// crashes\ next to the exe, or %TEMP% if the game folder is not writable
	char dir[MAX_PATH], base[MAX_PATH], txtPath[MAX_PATH], dmpPath[MAX_PATH];
	GetModuleFileNameA(NULL, dir, sizeof(dir));
	char* slash = strrchr(dir, '\\');
	strcpy(slash ? slash + 1 : dir, "crashes");
	CreateDirectoryA(dir, NULL);
	SYSTEMTIME t;
	GetLocalTime(&t);
	_snprintf(base, sizeof(base) - 1, "crash-%04d%02d%02d-%02d%02d%02d", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
	base[sizeof(base) - 1] = 0;
	_snprintf(txtPath, sizeof(txtPath) - 1, "%s\\%s.txt", dir, base);
	FILE* f = fopen(txtPath, "w");
	if (!f) {
		GetTempPathA(sizeof(dir), dir);
		_snprintf(txtPath, sizeof(txtPath) - 1, "%s%s.txt", dir, base);
		f = fopen(txtPath, "w");
	}
	txtPath[sizeof(txtPath) - 1] = 0;
	strcpy(dmpPath, txtPath);
	strcpy(dmpPath + strlen(dmpPath) - 3, "dmp");

	char where[MAX_PATH + 32], summary[512];
	DescribeAddress((DWORD)er->ExceptionAddress, where, sizeof(where));
	if (er->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && er->NumberParameters >= 2)
		_snprintf(summary, sizeof(summary) - 1, "%s (%s address 0x%08X) at %s", ExceptionName(er->ExceptionCode),
			er->ExceptionInformation[0] == 1 ? "write to" : er->ExceptionInformation[0] == 8 ? "execute" : "read from",
			(DWORD)er->ExceptionInformation[1], where);
	else
		_snprintf(summary, sizeof(summary) - 1, "%s (0x%08X) at %s", ExceptionName(er->ExceptionCode), er->ExceptionCode, where);
	summary[sizeof(summary) - 1] = 0;

	if (f) {
		char exe[MAX_PATH];
		GetModuleFileNameA(NULL, exe, sizeof(exe));
		fprintf(f, "CoD 1.5x %d crash report\n%04d-%02d-%02d %02d:%02d:%02d\n%s\n\n%s\n\n", BUILDNUMBER,
			t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond, exe, summary);
		fprintf(f, "EAX=%08X EBX=%08X ECX=%08X EDX=%08X\nESI=%08X EDI=%08X EBP=%08X ESP=%08X EIP=%08X\n\n",
			c->Eax, c->Ebx, c->Ecx, c->Edx, c->Esi, c->Edi, c->Ebp, c->Esp, c->Eip);

		// values on the stack that point into a loaded module: likely return addresses, newest first
		fprintf(f, "Stack (addresses inside modules):\n");
		DWORD stackTop = __readfsdword(4); // NT_TIB.StackBase of this thread
		int shown = 0;
		for (DWORD* p = (DWORD*)c->Esp; (DWORD)(p + 1) <= stackTop && p < (DWORD*)c->Esp + 4096 && shown < 48; p++) {
			char at[MAX_PATH + 32];
			DescribeAddress(*p, at, sizeof(at));
			if (at[0] != '0' || at[1] != 'x') {
				fprintf(f, "  [ESP+0x%04X] %s\n", (DWORD)((char*)p - (char*)c->Esp), at);
				shown++;
			}
		}
		fclose(f);
	}

	HANDLE dmp = CreateFileA(dmpPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (dmp != INVALID_HANDLE_VALUE) {
		MINIDUMP_EXCEPTION_INFORMATION mei = { GetCurrentThreadId(), ep, FALSE };
		MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), dmp, MiniDumpNormal, &mei, NULL, NULL);
		CloseHandle(dmp);
	}

	if (getenv("COD15X_CRASH_NODIALOG")) // automated test: files only
		return EXCEPTION_EXECUTE_HANDLER;

	// a fullscreen game would hide the message box: restore the desktop and the cursor first
	ChangeDisplaySettingsA(NULL, 0);
	EnumWindows(MinimizeOwnWindow, 0);
	ClipCursor(NULL);
	while (ShowCursor(TRUE) < 0) {}

	char msg[2048];
	_snprintf(msg, sizeof(msg) - 1,
		"Call of Duty crashed.\n\n%s\n\nA crash report was saved to:\n%s\n\n"
		"Please attach it to an issue at github.com/JotaJazz/CoD-1.5x.\nOpen the folder now?", summary, f ? txtPath : "(could not write the report)");
	msg[sizeof(msg) - 1] = 0;
	if (MessageBoxA(NULL, msg, "CoD 1.5x crash", MB_YESNO | MB_ICONERROR | MB_TOPMOST | MB_SETFOREGROUND) == IDYES && f) {
		char args[MAX_PATH + 16];
		_snprintf(args, sizeof(args) - 1, "/select,\"%s\"", txtPath);
		args[sizeof(args) - 1] = 0;
		ShellExecuteA(NULL, "open", "explorer.exe", args, NULL, SW_SHOWNORMAL);
	}

	return EXCEPTION_EXECUTE_HANDLER; // ends the process without the Windows error dialog
}

void Crash_Init() {
	SetUnhandledExceptionFilter(Crash_Filter);
}
