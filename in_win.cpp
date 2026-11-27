#include "shared.h"
#include "client.h"

// Raw mouse input (m_rawinput 1): the stock client reads the Windows cursor (GetCursorPos), so
// Windows pointer speed and "enhance pointer precision" change the aim. WM_INPUT gives the
// device counts instead.

#define mouseActive (*(int*)0x93B3A0)
#define windowCenterX (*(int*)0x93B398)
#define windowCenterY (*(int*)0x93B228)

static cvar_t* m_rawinput;
static WNDPROC origWndProc;
static int rawDx, rawDy;

static LRESULT CALLBACK RawInput_WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	if (msg == WM_INPUT && mouseActive) {
		RAWINPUT ri;
		UINT size = sizeof(ri);
		if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, &ri, &size, sizeof(RAWINPUTHEADER)) != (UINT)-1
			&& ri.header.dwType == RIM_TYPEMOUSE && !(ri.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE)) {
			rawDx += ri.data.mouse.lLastX;
			rawDy += ri.data.mouse.lLastY;
		}
	}
	return CallWindowProcA(origWndProc, hWnd, msg, wParam, lParam);
}

// called after every renderer init: the game window is recreated on vid_restart
void IN_InitRawInput(HWND hWnd) {
	if (!m_rawinput)
		m_rawinput = Cvar_Get("m_rawinput", "1", CVAR_ARCHIVE);

	if (!hWnd)
		return;

	if ((WNDPROC)GetWindowLongA(hWnd, GWL_WNDPROC) != RawInput_WndProc)
		origWndProc = (WNDPROC)SetWindowLongA(hWnd, GWL_WNDPROC, (LONG)RawInput_WndProc);

	RAWINPUTDEVICE rid = { 1, 2, 0, hWnd }; // generic desktop / mouse, legacy messages kept
	RegisterRawInputDevices(&rid, 1, sizeof(rid));
}

static void Sys_QueueMouseEvent(int deltaX, int deltaY) {
	// Sys_QueueEvent: time in eax, then type, value, value2, ptrLength, ptr (cdecl).
	// Don't name C variables like registers (dx, ax, ...): inline asm would push the register.
	__asm {
		push 0
		push 0
		push deltaY
		push deltaX
		push 3 // SE_MOUSE
		xor eax, eax
		mov ecx, 0x468930
		call ecx
		add esp, 0x14
	}
}

void IN_MouseMove() {
	if (!m_rawinput || !m_rawinput->integer || !origWndProc) {
		rawDx = rawDy = 0;
		void(*IN_Win32MouseMove)() = (void(*)())0x466B70;
		IN_Win32MouseMove();
		return;
	}

	SetCursorPos(windowCenterX, windowCenterY); // keep the hidden cursor away from the window edges

	int dx = rawDx, dy = rawDy;
	rawDx = rawDy = 0;
	if (dx || dy)
		Sys_QueueMouseEvent(dx, dy);
}
