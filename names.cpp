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
