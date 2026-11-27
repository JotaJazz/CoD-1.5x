// stdafx.h : common includes and helpers shared by every source file

#pragma once

#include "targetver.h"
#define __TITLE "Call of Duty Extended"

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <cstdint>
#include <ctime>

#include "mem_util.h"

#define QDECL __cdecl
typedef unsigned char byte;

#define DLL_EXPORT __declspec(dllexport)

static void MsgBox(const char *msg) {
	MessageBoxA(NULL, msg, __TITLE, MB_OK | MB_ICONINFORMATION);
}

typedef enum {
	COD_UNKNOWN,
	COD_5,
} cod_v;

extern int codversion;

static bool is_addr_safe(size_t addr) {
	__try {
		*(unsigned char*)(addr);
	} __except (1) {
		return false;
	}
	return true;
}
