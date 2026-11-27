// Self-check for sha256.cpp. Build from a VS x86 prompt in this folder:
//   cl /EHsc /D_CRT_SECURE_NO_WARNINGS sha256_test.cpp ..\sha256.cpp && sha256_test.exe
// (writes abc.txt / empty.txt next to the exe)
#include <stdio.h>
#include <string.h>
#include <assert.h>

bool Sys_FileSha256(const char* path, char hex[65]);

int main() {
	char h[65];
	FILE* f = fopen("abc.txt", "wb"); fputs("abc", f); fclose(f);
	f = fopen("empty.txt", "wb"); fclose(f);

	assert(Sys_FileSha256("abc.txt", h) && !strcmp(h, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
	assert(Sys_FileSha256("empty.txt", h) && !strcmp(h, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
	assert(!Sys_FileSha256("missing.txt", h));
	printf("ok\n");
	return 0;
}
