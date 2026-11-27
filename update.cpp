#include <windows.h>
#include <winhttp.h>
#include <string.h>
#include <stdlib.h>
#include "version.h"
#pragma comment(lib, "winhttp.lib")

// Update check: asks the GitHub API for the latest release once per start, on its own thread.
// Only reads the tag name (vNNNN); nothing is downloaded or sent besides the HTTPS request itself.
// Win32 only, no game calls (the game side is in cl_main.cpp).

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

static volatile LONG latestBuild; // 0 until the check finishes

static DWORD WINAPI Update_Thread(LPVOID) {
	char body[65536];
	DWORD total = 0;
	bool ok = false;

	HINTERNET session = WinHttpOpen(L"CoD-1.5x/" BUILD, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	HINTERNET conn = session ? WinHttpConnect(session, L"api.github.com", INTERNET_DEFAULT_HTTPS_PORT, 0) : NULL;
	HINTERNET req = conn ? WinHttpOpenRequest(conn, L"GET", L"/repos/JotaJazz/CoD-1.5x/releases/latest", NULL,
		WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE) : NULL;
	if (req) {
		WinHttpSetTimeouts(req, 5000, 5000, 5000, 5000);
		DWORD status = 0, size = sizeof(status);
		ok = WinHttpSendRequest(req, L"Accept: application/vnd.github+json\r\n", (DWORD)-1L, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)
			&& WinHttpReceiveResponse(req, NULL)
			&& WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL, &status, &size, NULL)
			&& status == 200;
		DWORD read = 0;
		while (ok && total < sizeof(body) - 1 && WinHttpReadData(req, body + total, sizeof(body) - 1 - total, &read) && read)
			total += read;
	}
	body[total] = 0;
	if (req) WinHttpCloseHandle(req);
	if (conn) WinHttpCloseHandle(conn);
	if (session) WinHttpCloseHandle(session);

	if (ok)
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
