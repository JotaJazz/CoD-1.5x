// Crash handler test: crashes on purpose; run_tests.bat checks that crashes\crash-*.txt and .dmp appear.
// Build: cl /EHsc /D_CRT_SECURE_NO_WARNINGS crash_test.cpp ..\crash.cpp
#include <stdlib.h>

void Crash_Init();

int main() {
	_putenv("COD15X_CRASH_NODIALOG=1");
	Crash_Init();
	volatile int* p = 0;
	*p = 1; // ACCESS_VIOLATION write to 0x00000000
	return 0;
}
