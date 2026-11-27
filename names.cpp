#include "shared.h"

// Server, map and gametype names for Discord. Pure string code (no game calls), so tests/unit_test.cpp can link it.

char* Q_CleanStr(char* string, bool colors) {
	char* d;
	char* s;
	int c;

	s = string;
	d = string;
	while ((c = *s) != 0) {
		if (Q_IsColorString(s) && !colors) {
			s++;
		}
		else if (c >= 0x20 && c <= 0x7E) {
			*d++ = c;
		}
		s++;
	}
	*d = '\0';

	return string;
}

// Cleans the hostname in place (size = size of the buffer).
char* Com_CleanHostname(char* string, int size, bool colors) {
	Q_CleanStr(string, colors); // printable ASCII only, colors removed for RPC

	// leading spaces and the '!' / '\'' tricks used to sort first in server lists
	char* start = string;
	while (*start == ' ' || *start == '!' || *start == '\'')
		start++;
	memmove(string, start, strlen(start) + 1);

	char visible[1024]; // what is left without color codes
	strncpy(visible, string, sizeof(visible) - 1);
	visible[sizeof(visible) - 1] = 0;
	if (!*Q_CleanStr(visible, false) && size > 0) {
		strncpy(string, "Unnamed Server", size - 1);
		string[size - 1] = 0;
	}

	return string;
}

// mp_carentan -> Carentan, xp_my_map -> My Map (in place, returns a pointer into mapname)
char* Com_CleanMapname(char* mapname) {
	for (int i = 0; mapname[i]; i++)
		mapname[i] = tolower((unsigned char)mapname[i]);

	if (!strncmp(mapname, "mp_", 3) || !strncmp(mapname, "xp_", 3))
		mapname += 3;

	mapname[0] = toupper((unsigned char)mapname[0]);

	for (int i = 0; mapname[i]; i++) {
		if (mapname[i] == '_') {
			mapname[i] = ' ';
			mapname[i + 1] = toupper((unsigned char)mapname[i + 1]);
		}
	}

	return mapname;
}

const char* Com_GametypeName(char* gt, bool colors) { // Keep colors for loading screen, remove for RPC.
	static const char* stock[][2] = {
		{ "dm", "Deathmatch" }, { "tdm", "Team Deathmatch" }, { "re", "Retrieval" },
		{ "bel", "Behind Enemy Lines" }, { "sd", "Search & Destroy" },
	};

	if (!gt || !*gt) return "Unknown Gametype";

	for (int i = 0; i < sizeof(stock) / sizeof(stock[0]); i++)
		if (!strcmp(gt, stock[i][0]))
			return stock[i][1];

	return colors ? gt : Q_CleanStr(gt);
}

// Server downloads are written under the game folder with the name the server sends. Accept only
// "<folder>/<name>.pk3": no "..", drive letters, backslashes or subfolders that could write elsewhere.
bool Com_IsSafeDownloadName(const char* name) {
	size_t len = name ? strlen(name) : 0;
	if (len < 6 || len >= 256 || _stricmp(name + len - 4, ".pk3"))
		return false;
	if (strstr(name, "..") || strpbrk(name, ":\\") || name[0] == '/')
		return false;
	const char* slash = strchr(name, '/');
	return slash && slash[1] != '.' && !strchr(slash + 1, '/'); // exactly one folder, no "folder/.pk3"
}

// "host" or "host:port" -> host (in a buffer of hostSize) and the port, or defaultPort; 0 if invalid.
int Com_ParseHostPort(const char* address, char* host, int hostSize, int defaultPort) {
	if (!address || hostSize <= 0)
		return 0;
	const char* colon = strchr(address, ':');
	int length = colon ? (int)(colon - address) : (int)strlen(address);
	int port = colon ? atoi(colon + 1) : defaultPort;
	if (length <= 0 || length >= hostSize || port <= 0 || port > 65535)
		return 0;
	memcpy(host, address, length);
	host[length] = 0;
	return port;
}

// Chat arrives as the server command  h "\x15Name^7: text"  (i for team chat). Returns the command with
// "time " put at the start of the message, in out, or cmd itself when it is not chat or does not fit.
const char* Com_AddChatTime(const char* cmd, const char* time, char* out, int size) {
	if (!cmd || (cmd[0] != 'h' && cmd[0] != 'i') || cmd[1] != ' ' || cmd[2] != '"')
		return cmd;
	int prefix = cmd[3] == '\x15' ? 4 : 3; // keep the engine's marker in front
	if ((int)(strlen(cmd) + strlen(time) + 2) > size)
		return cmd;
	_snprintf(out, size, "%.*s%s %s", prefix, cmd, time, cmd + prefix);
	out[size - 1] = 0;
	return out;
}

// Console history is saved to a text file: leave out empty lines and anything that can carry a password.
bool Con_IsSavedHistoryLine(const char* line) {
	if (!line)
		return false;
	while (*line == ' ' || *line == '\\' || *line == '/')
		line++;
	if (!*line || !_strnicmp(line, "rcon", 4))
		return false;
	for (const char* p = line; *p; p++)
		if (!_strnicmp(p, "password", 8))
			return false;
	return true;
}
