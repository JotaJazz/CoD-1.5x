#include <windows.h>
#include <winhttp.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "version.h"
#pragma comment(lib, "winhttp.lib")

// Update check: asks the GitHub API for the latest release once per start, on its own thread, and
// reads the tag name (vNNNN) and the SHA-256 of cod15x.dll from the release notes. Nothing else is
// downloaded until the player types /update (Update_Install). Win32 only, no game calls (the game
// side is in cl_main.cpp).

// "tag_name": "v2045" -> 2045; 0 if missing or malformed
int ParseLatestBuild(const char* json) {
	const char* p = json ? strstr(json, "\"tag_name\"") : NULL;
	if (!p) return 0;
	p += strlen("\"tag_name\"");
	while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
	if (*p++ != ':') return 0;
	while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
	if (*p++ != '"') return 0;
	if (*p == 'v' || *p == 'V') p++;
	if (*p < '0' || *p > '9') return 0;
	return atoi(p);
}

// "SHA-256 ... `<64 hex>`" in the release notes (written by the CI) -> lowercase hex; false if missing
bool ParseReleaseSha256(const char* json, char hex[65]) {
	const char* p = json ? strstr(json, "SHA-256") : NULL;
	for (; p && *p; p++) {
		int n = 0;
		while (n < 64 && ((p[n] >= '0' && p[n] <= '9') || ((p[n] | 0x20) >= 'a' && (p[n] | 0x20) <= 'f')))
			n++;
		bool boundary = !((p[n] >= '0' && p[n] <= '9') || ((p[n] | 0x20) >= 'a' && (p[n] | 0x20) <= 'z'));
		if (n == 64 && boundary) {
			for (int i = 0; i < 64; i++)
				hex[i] = p[i] | 0x20;
			hex[64] = 0;
			return true;
		}
		if (n)
			p += n - 1;
	}
	return false;
}

static volatile LONG latestBuild; // 0 until the check finishes
static char latestSha256[65];      // set before latestBuild, empty if the notes have none

// HTTPS GET into buf (size bytes); returns the bytes read, or -1 on any error or non-200 answer.
static int Http_Get(const wchar_t* host, const wchar_t* path, const wchar_t* headers, char* buf, int size) {
	int total = -1;
	HINTERNET session = WinHttpOpen(L"CoD-1.5x/" BUILD, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	HINTERNET conn = session ? WinHttpConnect(session, host, INTERNET_DEFAULT_HTTPS_PORT, 0) : NULL;
	HINTERNET req = conn ? WinHttpOpenRequest(conn, L"GET", path, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE) : NULL;
	if (req) {
		WinHttpSetTimeouts(req, 5000, 5000, 10000, 10000);
		DWORD status = 0, statusSize = sizeof(status);
		if (WinHttpSendRequest(req, headers ? headers : WINHTTP_NO_ADDITIONAL_HEADERS, headers ? (DWORD)-1L : 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)
			&& WinHttpReceiveResponse(req, NULL) // follows GitHub's redirect to its download host
			&& WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL, &status, &statusSize, NULL)
			&& status == 200) {
			DWORD read = 0;
			total = 0;
			while (total < size && WinHttpReadData(req, buf + total, size - total, &read) && read)
				total += read;
			if (total == size) // did not fit: treat as an error rather than use a truncated file
				total = -1;
		}
	}
	if (req) WinHttpCloseHandle(req);
	if (conn) WinHttpCloseHandle(conn);
	if (session) WinHttpCloseHandle(session);
	return total;
}

static DWORD WINAPI Update_Thread(LPVOID) {
	static char body[65536];
	int total = Http_Get(L"api.github.com", L"/repos/JotaJazz/CoD-1.5x/releases/latest", L"Accept: application/vnd.github+json\r\n", body, sizeof(body) - 1);
	if (total < 0)
		return 0;
	body[total] = 0;
	if (!ParseReleaseSha256(body, latestSha256))
		latestSha256[0] = 0;
	InterlockedExchange(&latestBuild, ParseLatestBuild(body));
	return 0;
}

void Update_Start() {
	HANDLE thread = CreateThread(NULL, 0, Update_Thread, NULL, 0, NULL);
	if (thread) CloseHandle(thread);
}

// latest released build number, 0 while unknown
int Update_LatestBuild() {
	return InterlockedCompareExchange(&latestBuild, 0, 0);
}

// /update: download cod15x.dll of the latest release, check its SHA-256 against the release notes
// and put it in place of our own DLL. Windows lets a loaded DLL be renamed, so the running copy
// becomes <name>.old (deleted at the next start, Update_Cleanup) and the new one is used after a
// restart. Runs on its own thread; Update_Status() tells the game how it went.
static volatile LONG installState; // 0 idle, 1 running, 2 done, 3 failed
static char installMessage[256];

static void Update_Fail(const char* message) {
	strncpy(installMessage, message, sizeof(installMessage) - 1);
	InterlockedExchange(&installState, 3);
}

static bool Update_OwnPath(char* path, int size) {
	HMODULE self;
	return GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)Update_OwnPath, &self)
		&& GetModuleFileNameA(self, path, size) > 0;
}

static DWORD WINAPI Update_InstallThread(LPVOID) {
	bool Sys_FileSha256(const char* path, char hex[65]);
	int build = InterlockedCompareExchange(&latestBuild, 0, 0);
	char self[MAX_PATH], fresh[MAX_PATH + 8], old[MAX_PATH + 8], hash[65];
	if (!Update_OwnPath(self, sizeof(self)))
		return Update_Fail("can't find the CoD 1.5x DLL"), 0;
	_snprintf(fresh, sizeof(fresh), "%s.new", self);
	_snprintf(old, sizeof(old), "%s.old", self);

	const int maxSize = 8 << 20;
	char* dll = (char*)malloc(maxSize);
	wchar_t path[128];
	_snwprintf(path, 128, L"/JotaJazz/CoD-1.5x/releases/download/v%d/cod15x.dll", build);
	int size = dll ? Http_Get(L"github.com", path, NULL, dll, maxSize) : -1;
	if (size < 4096 || dll[0] != 'M' || dll[1] != 'Z') {
		free(dll);
		return Update_Fail("download failed (no cod15x.dll in the release?)"), 0;
	}
	FILE* f = fopen(fresh, "wb");
	bool written = f && fwrite(dll, 1, size, f) == (size_t)size;
	if (f) fclose(f);
	free(dll);
	if (!written) {
		DeleteFileA(fresh);
		return Update_Fail("can't write to the game folder: close the game and run install.bat"), 0;
	}
	if (!Sys_FileSha256(fresh, hash) || strcmp(hash, latestSha256)) {
		DeleteFileA(fresh);
		return Update_Fail("the download does not match the SHA-256 of the release, nothing was changed"), 0;
	}

	DeleteFileA(old); // a leftover from an update in this same session
	if (!MoveFileExA(self, old, MOVEFILE_REPLACE_EXISTING)) {
		DeleteFileA(fresh);
		return Update_Fail("can't replace the DLL (game in Program Files?): close the game and run install.bat"), 0;
	}
	if (!MoveFileExA(fresh, self, MOVEFILE_REPLACE_EXISTING)) {
		MoveFileExA(old, self, MOVEFILE_REPLACE_EXISTING); // put the running one back
		DeleteFileA(fresh);
		return Update_Fail("can't replace the DLL: close the game and run install.bat"), 0;
	}
	_snprintf(installMessage, sizeof(installMessage) - 1, "CoD 1.5x %d installed. Restart the game to use it.", build);
	InterlockedExchange(&installState, 2);
	return 0;
}

// false if there is nothing to install (no newer release, or its notes have no SHA-256) or it already runs
bool Update_Install() {
	if (Update_LatestBuild() <= BUILDNUMBER || !latestSha256[0])
		return false;
	if (InterlockedCompareExchange(&installState, 1, 0) != 0 && InterlockedCompareExchange(&installState, 1, 3) != 3)
		return false;
	HANDLE thread = CreateThread(NULL, 0, Update_InstallThread, NULL, 0, NULL);
	if (!thread)
		return Update_Fail("can't start the download"), true;
	CloseHandle(thread);
	return true;
}

// 2 done / 3 failed with a message once (then 0 until the next try), 1 running, 0 idle
int Update_Status(const char** message) {
	LONG state = InterlockedCompareExchange(&installState, 0, 0);
	*message = installMessage;
	if (state == 2 || state == 3)
		InterlockedExchange(&installState, state == 2 ? 4 : 0); // 4: done, report once and stay installed
	return state;
}

// the copy an earlier /update left behind, now that it is no longer loaded
void Update_Cleanup() {
	char self[MAX_PATH], old[MAX_PATH + 8];
	if (Update_OwnPath(self, sizeof(self))) {
		_snprintf(old, sizeof(old), "%s.old", self);
		DeleteFileA(old);
	}
}
