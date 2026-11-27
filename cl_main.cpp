#include "shared.h"
#include "client.h"
#include "version.h"

cvar_t* cl_running;

void CL_FOVLimit() {
	int fov = Cvar_VariableIntegerValue("cg_fov");
	int cheats = atoi(Info_ValueForKey(cs1, "sv_cheats"));

	char* sv_fov_min = Info_ValueForKey(cs1, "sv_fov_min");
	char* sv_fov_max = Info_ValueForKey(cs1, "sv_fov_max");
	int fovMin = strlen(sv_fov_min) ? atoi(sv_fov_min) : 80;
	int fovMax = strlen(sv_fov_max) ? atoi(sv_fov_max) : 95;

	if ((fov < fovMin || fov > fovMax) && cheats != 1) {
		Com_Printf("cg_fov \"%d\" is invalid. Allowed values: \"%d\" - \"%d\".\n", fov, fovMin, fovMax);
		Cvar_Set("cg_fov", "80");
	}
}

void CL_Frame(int msec) {

	if (!cl_running->integer) return;
	
	void CL_DiscordFrame();
	CL_DiscordFrame();

	CL_FOVLimit();

	// update check result (update.cpp): show it in the version text of the menus, once
	static bool updateAnnounced;
	int Update_LatestBuild();
	int latest = Update_LatestBuild();
	if (!updateAnnounced && latest > BUILDNUMBER) {
		updateAnnounced = true;
		Com_Printf("^3CoD 1.5x %d is available: type ^7/update^3 to install it, or get it from https://github.com/JotaJazz/CoD-1.5x/releases\n", latest);
		Cvar_Set("shortversion", va("1.5x (%d) - %d available, /update", BUILDNUMBER, latest));
	}

	const char* updateMessage;
	int Update_Status(const char** message);
	switch (Update_Status(&updateMessage)) {
	case 2:
		Com_Printf("^2%s\n", updateMessage);
		Cvar_Set("shortversion", va("1.5x (%d) - restart to update", BUILDNUMBER));
		break;
	case 3:
		Com_Printf("^1Update failed: %s\n", updateMessage);
		break;
	}
}
UINT_PTR pcl_frame = (UINT_PTR)0x412710;
void __declspec(naked) CL_FrameStub()
{
	__asm
	{
		push    eax
		call	pcl_frame
		call    CL_Frame
		add     esp, 4
		retn
	}
}

// CL_NextDownload -> CL_BeginDownload(localName in eax, remoteName on the stack): the local name comes
// from the server and decides where the file is written. Drop the connection instead of writing outside.
static void __cdecl CL_CheckDownloadName(const char* localName) {
	bool Com_IsSafeDownloadName(const char* name);
	if (!Com_IsSafeDownloadName(localName))
		Com_Error(ERR_DROP, "Server tried to download a file with an unsafe name:\n%.100s", localName ? localName : "");
}

__declspec(naked) void CL_BeginDownloadStub() {
	__asm {
		push eax
		push eax
		call CL_CheckDownloadName
		add esp, 4
		pop eax
		mov ecx, 0x410D50 // CL_BeginDownload
		jmp ecx
	}
}

// The server list only asks codmaster.activision.com. Send the same getservers query to a second
// master (cl_masterServer2, empty = off) so the list still fills if Activision's goes away; the engine
// merges both answers and skips servers it already has.
typedef void(__cdecl* NET_OutOfBandPrint_t)(int sock, netadr_t adr, const char* format, ...);
static void __cdecl CL_RequestServers(int sock, netadr_t adr, const char* query) {
	NET_OutOfBandPrint_t NET_OutOfBandPrint = (NET_OutOfBandPrint_t)0x44C290;
	NET_OutOfBandPrint(sock, adr, query); // the engine passes the query as the format, keep that

	char host[256];
	int Com_ParseHostPort(const char* address, char* host, int hostSize, int defaultPort);
	int port = Com_ParseHostPort(Cvar_Get("cl_masterServer2", "master.cod.pm", CVAR_ARCHIVE)->string, host, sizeof(host), 20510);
	if (!port)
		return;
	hostent* h = gethostbyname(host); // blocks like the engine's own lookup of the Activision master
	if (!h || h->h_addrtype != AF_INET) {
		Com_Printf("Couldn't resolve cl_masterServer2 %s\n", host);
		return;
	}
	netadr_t master = { NA_IP };
	memcpy(master.ip, h->h_addr_list[0], 4);
	master.port = htons((u_short)port);
	Com_Printf("Requesting servers from %s...\n", host);
	NET_OutOfBandPrint(sock, master, query);
}

// Time of day in front of chat messages (cg_chattime), in the chat area and the console alike.
static const char* __cdecl CL_ChatTime(const char* cmd) {
	static char out[1100]; // a server command is at most 1024
	static cvar_t* cg_chattime;
	if (!cg_chattime)
		cg_chattime = Cvar_Get("cg_chattime", "1", CVAR_ARCHIVE);
	if (!cg_chattime->integer)
		return cmd;
	SYSTEMTIME now;
	GetLocalTime(&now);
	char time[16];
	_snprintf(time, sizeof(time), "^7[%02d:%02d]", now.wHour, now.wMinute);
	time[sizeof(time) - 1] = 0;
	const char* Com_AddChatTime(const char* cmd, const char* time, char* out, int size);
	return Com_AddChatTime(cmd, time, out, sizeof(out));
}

// CL_GetServerCommand: Cmd_TokenizeString(command in eax, one stack arg) before cgame reads it
__declspec(naked) void CL_TokenizeServerCommandStub() {
	__asm {
		push eax
		call CL_ChatTime
		add esp, 4
		mov ecx, 0x42A700 // Cmd_TokenizeString
		jmp ecx
	}
}

// Console history (up/down arrow) survives restarts: Main\console_history.txt, saved on quit.
// The engine keeps 32 field_t entries (0x11c bytes, text at +0x1c) and copies one into the console
// field on recall, so each loaded entry is built the way the engine resets the field after Enter.
#define CON_HISTORY ((BYTE*)0x14896E0)
#define CON_HISTORY_SIZE 32
#define CON_FIELD_SIZE 0x11C
#define CON_NEXT_HISTORY (*(int*)0x148884C)
#define CON_HISTORY_LINE (*(int*)0x1488844)

static void Con_HistoryPath(char* path, int size) {
	GetModuleFileNameA(NULL, path, size);
	char* slash = strrchr(path, '\\');
	_snprintf(slash ? slash + 1 : path, size - (slash ? (int)(slash + 1 - path) : 0) - 1, "Main\\console_history.txt");
	path[size - 1] = 0;
}

void Con_SaveHistory() {
	bool Con_IsSavedHistoryLine(const char* line);
	char path[MAX_PATH];
	Con_HistoryPath(path, sizeof(path));
	FILE* f = fopen(path, "w");
	if (!f)
		return;
	int first = max(0, CON_NEXT_HISTORY - CON_HISTORY_SIZE);
	for (int i = first; i < CON_NEXT_HISTORY; i++) {
		const char* line = (const char*)CON_HISTORY + (i % CON_HISTORY_SIZE) * CON_FIELD_SIZE + 0x1C;
		if (Con_IsSavedHistoryLine(line))
			fprintf(f, "%.255s\n", line);
	}
	fclose(f);
}

static void Con_LoadHistory() {
	char path[MAX_PATH], line[512];
	Con_HistoryPath(path, sizeof(path));
	FILE* f = fopen(path, "r");
	if (!f)
		return;
	int count = 0;
	while (count < CON_HISTORY_SIZE && fgets(line, sizeof(line), f)) {
		line[strcspn(line, "\r\n")] = 0;
		if (!*line)
			continue;
		BYTE* field = CON_HISTORY + count * CON_FIELD_SIZE;
		memset(field, 0, CON_FIELD_SIZE);
		strncpy((char*)field + 0x1C, line, 255);
		((int*)field)[0] = strlen((char*)field + 0x1C); // cursor at the end
		((int*)field)[2] = 0x100;                       // as after Enter (CL_KeyDownEvent, 0x40DF01)
		((int*)field)[3] = *(int*)0x5BCD38;
		((int*)field)[4] = *(int*)0x5BCD3C;
		((int*)field)[5] = *(int*)0x5BCD40;
		((int*)field)[6] = 1;
		count++;
	}
	fclose(f);
	CON_NEXT_HISTORY = CON_HISTORY_LINE = count;
}

void Cmd_Update() {
	bool Update_Install();
	int Update_LatestBuild();
	if (Update_Install())
		Com_Printf("Downloading CoD 1.5x %d...\n", Update_LatestBuild());
	else if (Update_LatestBuild() > BUILDNUMBER)
		Com_Printf("The update is already running or installed, or this release can't be installed from the game: get it from https://github.com/JotaJazz/CoD-1.5x/releases\n");
	else if (Update_LatestBuild())
		Com_Printf("CoD 1.5x %d is the latest version.\n", BUILDNUMBER);
	else
		Com_Printf("No update information yet (cl_updatecheck 1 checks GitHub at startup).\n");
}

void Cmd_Borderless() {
	if (Cmd_Argc() > 1) {
		Cvar_Set("r_borderless", Cmd_Argv(1));

		if (Cvar_VariableIntegerValue("r_borderless")) {
			Cvar_Set("r_fullscreen", "0");
			Cvar_Set("r_mode", "11"); // native desktop resolution, see R_PatchNativeMode
			Cvar_Set("com_introplayed", "1");
		}

		void(*Cbuf_ExecuteText)(const char*);
		*(UINT32*)&Cbuf_ExecuteText = 0x42AAB0;
		Cbuf_ExecuteText("vid_restart");
	}
	else {
		Com_Printf("\"r_borderless\" is:\"%d\" default: \"0\"\n", Cvar_VariableIntegerValue("r_borderless"));
	}
}

void CL_Init(void) {
	void(*oCL_Init)();
	*(int*)(&oCL_Init) = 0x413380;
	oCL_Init();

	cl_running = Cvar_Get("cl_running", "0", CVAR_ROM);
	Cvar_Get("cg_fov", "80", CVAR_ARCHIVE);
	Cvar_Get("cl_allowCustomDlls", "0", CVAR_ARCHIVE); // see librarymodule.cpp
	Cvar_Get("r_borderless", "0", CVAR_ARCHIVE); // read by R_Init on the next start
	Cvar_Get("cl_masterServer2", "master.cod.pm", CVAR_ARCHIVE); // see CL_RequestServers
	extern cvar_t* cg_widescreenhud;
	cg_widescreenhud = Cvar_Get("cg_widescreenhud", "1", CVAR_ARCHIVE); // see hud.cpp
	extern cvar_t* ui_widescreen;
	ui_widescreen = Cvar_Get("ui_widescreen", "1", CVAR_ARCHIVE);

	Cvar_Set("version", va("COD MP 1.5x build %d %s %s win-x86", BUILDNUMBER, __DATE__, __TIME__));
	Cvar_Set("shortversion", va("1.5x (%d)", BUILDNUMBER));
	// recommended settings are applied once; after that the player's config wins
	cvar_t* defaultsApplied = Cvar_Get("cl_xdefaults", "0", CVAR_ARCHIVE);
	if (!defaultsApplied->integer) {
		Cvar_Set("r_overbrightbits", "0");
		Cvar_Set("r_ignorehwgamma", "0");
		Cvar_Set("cl_languagewarnings", "0");
		Cvar_Set("cl_languagewarningsaserrors", "0");
		Cvar_Set("com_hunkmegs", "512");
		Cvar_Set("cl_xdefaults", "1");
	}

	Cmd_AddCommand("r_borderless", Cmd_Borderless);
	Cmd_AddCommand("update", Cmd_Update);

	static bool initOnce; // CL_Init runs again on some restarts
	if (!initOnce) {
		initOnce = true;
		Con_LoadHistory();
		void Update_Cleanup();
		Update_Cleanup();
		__call(0x415175, (int)CL_RequestServers); // NET_OutOfBandPrint in CL_GlobalServers_f
	}

	static bool updateStarted; // CL_Init runs again on some restarts
	if (!updateStarted && Cvar_Get("cl_updatecheck", "1", CVAR_ARCHIVE)->integer) {
		updateStarted = true;
		void Update_Start();
		Update_Start();
	}
}