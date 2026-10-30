// SPDX-License-Identifier: GPL-2.0-or-later
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "../src/license_check.h"

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmdLine, int nCmdShow) {
    // Delete test license so the dialog always shows during UI testing
    DeleteFileA("spartanport_license.key");
    DeleteFileA("user/spartanport_license.key");
    int res = license_check_prompt();
    return res;
}
