// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "license_check.h"

#define SECRET_SALT "PCPortOP_Demo_Secret_2026"

// Dark Theme Color Palette
#define BG_COLOR       RGB(14, 15, 18)      // Pitch dark obsidian
#define HEADER_BG      RGB(20, 22, 28)      // Sleek top header
#define CARD_BG        RGB(24, 27, 35)      // Distinct card background
#define INPUT_BG       RGB(28, 31, 40)      // High-contrast input box
#define BORDER_COLOR   RGB(55, 60, 75)      // Subtle dark border
#define ACCENT_GOLD    RGB(245, 175, 45)    // PCPortOP Spartan Gold
#define ACCENT_RED     RGB(220, 65, 30)     // Flame CTA Button
#define ACCENT_GREEN   RGB(50, 205, 100)    // Verification success green
#define TEXT_PRIMARY   RGB(230, 234, 242)   // Crisp readable light gray
#define TEXT_MUTED     RGB(145, 150, 168)   // Muted gray
#define BTN_DARK       RGB(34, 38, 48)      // Secondary dark button

// Control IDs
#define IDC_COPY_BTN    1001
#define IDC_PASTE_BTN   1002
#define IDC_ACT_BTN     1003
#define IDC_EXIT_BTN    1004
#define IDC_HWID_EDIT   1005
#define IDC_KEY_EDIT    1006
#define IDC_TG_BTN      1007
#define IDC_YT_BTN      1008
#define IDC_STATUS_TXT  1009

static char g_hwid[32] = {0};
static char g_license_path[MAX_PATH] = "spartanport_license.key";
static int g_activated = 0;
static HWND g_hKeyEdit = NULL;
static HWND g_hStatusText = NULL;

static HBRUSH g_hBgBrush = NULL;
static HBRUSH g_hCardBrush = NULL;
static HBRUSH g_hHeaderBrush = NULL;
static HBRUSH g_hInputBrush = NULL;

static HFONT g_hFontTitle = NULL;
static HFONT g_hFontHeading = NULL;
static HFONT g_hFontSub = NULL;
static HFONT g_hFontNormal = NULL;
static HFONT g_hFontMono = NULL;
static HFONT g_hFontSmall = NULL;

static COLORREF g_statusColor = RGB(145, 150, 168);

// FNV-1a 64-bit hash
static unsigned long long fnv1a64(const char* str) {
    unsigned long long hash = 14695981039346656037ULL;
    while (*str) {
        hash ^= (unsigned char)(*str++);
        hash *= 1099511628211ULL;
    }
    return hash;
}

static void GetMachineCode(char* out_code, size_t max_len) {
    DWORD serial = 0;
    if (!GetVolumeInformationW(L"C:\\", NULL, 0, &serial, NULL, NULL, NULL, 0)) {
        serial = 0x5043504F; // Fallback 'PCPO'
    }
    unsigned short p1 = (unsigned short)((serial >> 16) & 0xFFFF);
    unsigned short p2 = (unsigned short)(serial & 0xFFFF);
    snprintf(out_code, max_len, "GOW3-%04X-%04X", p1, p2);
}

static void GenerateKey(const char* hwid, char* out_key, size_t max_len) {
    char combined[128];
    snprintf(combined, sizeof(combined), "%s:%s", hwid, SECRET_SALT);
    unsigned long long h = fnv1a64(combined);
    unsigned short k1 = (unsigned short)((h >> 16) & 0xFFFF);
    unsigned short k2 = (unsigned short)(h & 0xFFFF);
    snprintf(out_key, max_len, "ACT-%04X-%04X", k1, k2);
}

static int VerifyKey(const char* hwid, const char* user_key) {
    char expected[32];
    GenerateKey(hwid, expected, sizeof(expected));
    char clean_user[32] = {0};
    int j = 0;
    for (int i = 0; user_key[i] && j < 31; ++i) {
        if (user_key[i] != ' ' && user_key[i] != '\t' && user_key[i] != '\r' && user_key[i] != '\n') {
            clean_user[j++] = (char)toupper((unsigned char)user_key[i]);
        }
    }
    return strcmp(clean_user, expected) == 0;
}

static const char* GetLicenseFilePath(void) {
    if (GetFileAttributesA("user") != INVALID_FILE_ATTRIBUTES) {
        return "user/spartanport_license.key";
    }
    return "spartanport_license.key";
}

static int IsActivated(const char* license_path, char* out_hwid) {
    GetMachineCode(out_hwid, 32);
    FILE* f = fopen(license_path, "r");
    if (!f) return 0;
    char saved_key[64] = {0};
    if (fgets(saved_key, sizeof(saved_key), f)) {
        fclose(f);
        return VerifyKey(out_hwid, saved_key);
    }
    fclose(f);
    return 0;
}

// Enable Windows 10/11 Dark Titlebar dynamically
static void EnableDarkModeTitle(HWND hwnd) {
    HMODULE hDwm = LoadLibraryA("dwmapi.dll");
    if (hDwm) {
        typedef HRESULT (WINAPI *DwmSetWindowAttribute_t)(HWND, DWORD, LPCVOID, DWORD);
        DwmSetWindowAttribute_t fn = (DwmSetWindowAttribute_t)GetProcAddress(hDwm, "DwmSetWindowAttribute");
        if (fn) {
            BOOL dark = TRUE;
            fn(hwnd, 20, &dark, sizeof(dark)); // DWMWA_USE_IMMERSIVE_DARK_MODE (Win11 / Win10 20H1+)
            fn(hwnd, 19, &dark, sizeof(dark)); // Fallback for Win10 1903/1909
        }
        FreeLibrary(hDwm);
    }
}

// Draw custom styled rounded button
static void DrawCustomButton(LPDRAWITEMSTRUCT pDIS, COLORREF bg_col, COLORREF border_col, COLORREF text_col, const wchar_t* text, HFONT hFont) {
    HDC hdc = pDIS->hDC;
    RECT rc = pDIS->rcItem;
    BOOL is_pressed = pDIS->itemState & ODS_SELECTED;

    if (is_pressed) {
        bg_col = RGB((BYTE)(GetRValue(bg_col) * 0.75), (BYTE)(GetGValue(bg_col) * 0.75), (BYTE)(GetBValue(bg_col) * 0.75));
    }

    HBRUSH hBr = CreateSolidBrush(bg_col);
    HPEN hPen = CreatePen(PS_SOLID, 1, is_pressed ? RGB(160, 165, 180) : border_col);
    HGDIOBJ oldBr = SelectObject(hdc, hBr);
    HGDIOBJ oldPen = SelectObject(hdc, hPen);

    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 6, 6);

    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBr);
    DeleteObject(hPen);
    DeleteObject(hBr);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, text_col);
    SelectObject(hdc, hFont);

    if (is_pressed) {
        OffsetRect(&rc, 1, 1);
    }
    DrawTextW(hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static void CopyToClipboard(HWND hwnd, const char* text) {
    if (!OpenClipboard(hwnd)) return;
    EmptyClipboard();
    size_t len = strlen(text) + 1;
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
    if (hMem) {
        memcpy(GlobalLock(hMem), text, len);
        GlobalUnlock(hMem);
        SetClipboardData(CF_TEXT, hMem);
    }
    CloseClipboard();
}

static void PasteFromClipboard(HWND hEdit) {
    if (!OpenClipboard(NULL)) return;
    HANDLE hData = GetClipboardData(CF_TEXT);
    if (hData) {
        char* text = (char*)GlobalLock(hData);
        if (text) {
            SetWindowTextA(hEdit, text);
            GlobalUnlock(hData);
        }
    }
    CloseClipboard();
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            EnableDarkModeTitle(hwnd);

            g_hFontTitle = CreateFontW(20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                       CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
            g_hFontHeading = CreateFontW(14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                         CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
            g_hFontSub = CreateFontW(13, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                     DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                     CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
            g_hFontNormal = CreateFontW(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
            g_hFontMono = CreateFontW(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                      CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_MODERN, L"Consolas");
            g_hFontSmall = CreateFontW(12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                       CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

            g_hBgBrush = CreateSolidBrush(BG_COLOR);
            g_hCardBrush = CreateSolidBrush(CARD_BG);
            g_hHeaderBrush = CreateSolidBrush(HEADER_BG);
            g_hInputBrush = CreateSolidBrush(INPUT_BG);

            // Step 1: HWID Edit Box + Copy Button
            HWND hHwidEdit = CreateWindowA("EDIT", g_hwid,
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_READONLY | ES_AUTOHSCROLL,
                28, 206, 350, 32, hwnd, (HMENU)IDC_HWID_EDIT, NULL, NULL);
            SendMessage(hHwidEdit, WM_SETFONT, (WPARAM)g_hFontMono, TRUE);

            CreateWindowW(L"BUTTON", L"📋 Copy Code",
                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                388, 206, 130, 32, hwnd, (HMENU)IDC_COPY_BTN, NULL, NULL);

            // Direct Bot & YouTube action links
            CreateWindowW(L"BUTTON", L"✈️ Open @PcPortOP_bot",
                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                28, 248, 240, 32, hwnd, (HMENU)IDC_TG_BTN, NULL, NULL);

            CreateWindowW(L"BUTTON", L"▶️ Visit YouTube Channel",
                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                278, 248, 240, 32, hwnd, (HMENU)IDC_YT_BTN, NULL, NULL);

            // Step 2: Key Edit Box + Paste Button
            g_hKeyEdit = CreateWindowA("EDIT", "",
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                28, 314, 350, 32, hwnd, (HMENU)IDC_KEY_EDIT, NULL, NULL);
            SendMessage(g_hKeyEdit, WM_SETFONT, (WPARAM)g_hFontMono, TRUE);

            CreateWindowW(L"BUTTON", L"📥 Paste Key",
                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                388, 314, 130, 32, hwnd, (HMENU)IDC_PASTE_BTN, NULL, NULL);

            // Inline Status Label (Replaces annoying alert boxes!)
            g_hStatusText = CreateWindowW(L"STATIC", L"Paste your key above, then click Unlock to play.",
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                28, 352, 490, 20, hwnd, (HMENU)IDC_STATUS_TXT, NULL, NULL);
            SendMessage(g_hStatusText, WM_SETFONT, (WPARAM)g_hFontSmall, TRUE);

            // Primary Action Buttons
            CreateWindowW(L"BUTTON", L"⚔️ UNLOCK & LAUNCH GAME",
                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                28, 380, 350, 44, hwnd, (HMENU)IDC_ACT_BTN, NULL, NULL);

            CreateWindowW(L"BUTTON", L"EXIT",
                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                388, 380, 130, 44, hwnd, (HMENU)IDC_EXIT_BTN, NULL, NULL);

            SetFocus(g_hKeyEdit);
            return 0;
        }

        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT pDIS = (LPDRAWITEMSTRUCT)lParam;
            if (pDIS->CtlID == IDC_COPY_BTN) {
                DrawCustomButton(pDIS, BTN_DARK, BORDER_COLOR, TEXT_PRIMARY, L"📋 Copy Code", g_hFontSub);
                return TRUE;
            } else if (pDIS->CtlID == IDC_PASTE_BTN) {
                DrawCustomButton(pDIS, BTN_DARK, BORDER_COLOR, TEXT_PRIMARY, L"📥 Paste Key", g_hFontSub);
                return TRUE;
            } else if (pDIS->CtlID == IDC_ACT_BTN) {
                DrawCustomButton(pDIS, ACCENT_RED, RGB(255, 110, 50), RGB(255, 255, 255), L"⚔️ UNLOCK & LAUNCH GAME", g_hFontHeading);
                return TRUE;
            } else if (pDIS->CtlID == IDC_EXIT_BTN) {
                DrawCustomButton(pDIS, BTN_DARK, BORDER_COLOR, TEXT_MUTED, L"EXIT", g_hFontSub);
                return TRUE;
            } else if (pDIS->CtlID == IDC_TG_BTN) {
                DrawCustomButton(pDIS, RGB(22, 38, 56), RGB(35, 95, 150), RGB(90, 195, 255), L"✈️ Open @PcPortOP_bot", g_hFontSub);
                return TRUE;
            } else if (pDIS->CtlID == IDC_YT_BTN) {
                DrawCustomButton(pDIS, RGB(48, 24, 26), RGB(140, 45, 50), RGB(255, 120, 120), L"▶️ Visit YouTube Channel", g_hFontSub);
                return TRUE;
            }
            break;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            RECT client_rc;
            GetClientRect(hwnd, &client_rc);

            FillRect(hdc, &client_rc, g_hBgBrush);

            // 1. Top Header Bar
            RECT header_rc = {0, 0, client_rc.right, 62};
            FillRect(hdc, &header_rc, g_hHeaderBrush);

            HPEN hBorderPen = CreatePen(PS_SOLID, 1, BORDER_COLOR);
            HPEN oldPen = (HPEN)SelectObject(hdc, hBorderPen);
            MoveToEx(hdc, 0, 62, NULL);
            LineTo(hdc, client_rc.right, 62);

            SetBkMode(hdc, TRANSPARENT);
            SelectObject(hdc, g_hFontTitle);
            SetTextColor(hdc, ACCENT_GOLD);
            TextOutW(hdc, 28, 10, L"⚔️ PCPORTOP", 12);

            SelectObject(hdc, g_hFontHeading);
            SetTextColor(hdc, TEXT_PRIMARY);
            TextOutW(hdc, 160, 12, L"— God of War III Remastered (V3)", 32);

            SelectObject(hdc, g_hFontSmall);
            SetTextColor(hdc, TEXT_MUTED);
            TextOutW(hdc, 30, 36, L"Community Build | 100% Crash-Proof, FSR 3 Upscaling & Clean Cutscenes", 70);

            // 2. Developer Note Card (y=72 to 166) - ROUNDED DARK CARD
            RECT card_rc = {26, 72, client_rc.right - 26, 166};
            
            HGDIOBJ oldBr = SelectObject(hdc, g_hCardBrush);
            RoundRect(hdc, card_rc.left, card_rc.top, card_rc.right, card_rc.bottom, 8, 8);

            // Left Gold Accent Bar
            RECT ribbon_rc = {card_rc.left + 2, card_rc.top + 3, card_rc.left + 6, card_rc.bottom - 3};
            HBRUSH hGoldBrush = CreateSolidBrush(ACCENT_GOLD);
            FillRect(hdc, &ribbon_rc, hGoldBrush);
            DeleteObject(hGoldBrush);

            // Card Text: Title
            SetBkMode(hdc, TRANSPARENT);
            SelectObject(hdc, g_hFontHeading);
            SetTextColor(hdc, ACCENT_GOLD);
            TextOutW(hdc, card_rc.left + 16, card_rc.top + 8, L"💡 Why This Verification?", 25);

            // Card Text: Message Body
            SelectObject(hdc, g_hFontNormal);
            SetTextColor(hdc, TEXT_PRIMARY);
            RECT msg_rc = {card_rc.left + 16, card_rc.top + 28, card_rc.right - 14, card_rc.bottom - 6};
            const wchar_t* note =
                L"I spent weeks researching, debugging, and testing dozens of builds until GOW3 ran 100% crash-proof on low-spec PCs. "
                L"Though offered money for this patch, I decided to share it completely free for our community. "
                L"This quick one-time key prevents unauthorized re-selling and helps grow our YouTube channel. Thank you!";
            DrawTextW(hdc, note, -1, &msg_rc, DT_LEFT | DT_WORDBREAK);

            // 3. Step Labels
            SelectObject(hdc, g_hFontSub);
            SetTextColor(hdc, TEXT_PRIMARY);
            TextOutW(hdc, 28, 186, L"STEP 1: Your Machine Code (Send to @PcPortOP_bot):", 50);
            TextOutW(hdc, 28, 294, L"STEP 2: Enter Your Activation Key:", 34);

            SelectObject(hdc, oldBr);
            SelectObject(hdc, oldPen);
            DeleteObject(hBorderPen);

            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_CTLCOLOREDIT: {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, ACCENT_GOLD);
            SetBkColor(hdc, INPUT_BG);
            return (LRESULT)g_hInputBrush;
        }

        case WM_CTLCOLORSTATIC: {
            HDC hdc = (HDC)wParam;
            HWND hStatic = (HWND)lParam;
            if (hStatic == g_hStatusText) {
                SetTextColor(hdc, g_statusColor);
            } else {
                SetTextColor(hdc, TEXT_PRIMARY);
            }
            SetBkMode(hdc, TRANSPARENT);
            return (LRESULT)g_hBgBrush;
        }

        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            if (wmId == IDC_COPY_BTN) {
                CopyToClipboard(hwnd, g_hwid);
                g_statusColor = ACCENT_GOLD;
                SetWindowTextW(g_hStatusText, L"📋 Machine code copied! Now send it to @PcPortOP_bot on Telegram.");
                InvalidateRect(g_hStatusText, NULL, TRUE);
            } else if (wmId == IDC_PASTE_BTN) {
                PasteFromClipboard(g_hKeyEdit);
                g_statusColor = TEXT_MUTED;
                SetWindowTextW(g_hStatusText, L"Key pasted. Click 'Unlock & Launch Game' below.");
                InvalidateRect(g_hStatusText, NULL, TRUE);
            } else if (wmId == IDC_TG_BTN) {
                ShellExecuteW(NULL, L"open", L"https://t.me/PcPortOP_bot", NULL, NULL, SW_SHOWNORMAL);
            } else if (wmId == IDC_YT_BTN) {
                ShellExecuteW(NULL, L"open", L"https://www.youtube.com/@pcportop", NULL, NULL, SW_SHOWNORMAL);
            } else if (wmId == IDC_EXIT_BTN) {
                DestroyWindow(hwnd);
            } else if (wmId == IDC_ACT_BTN) {
                char input_key[64] = {0};
                GetWindowTextA(g_hKeyEdit, input_key, sizeof(input_key));
                if (VerifyKey(g_hwid, input_key)) {
                    FILE* f = fopen(g_license_path, "w");
                    if (f) {
                        fprintf(f, "%s\n", input_key);
                        fclose(f);
                    }
                    g_activated = 1;
                    g_statusColor = ACCENT_GREEN;
                    SetWindowTextW(g_hStatusText, L"✅ Key verified! Game permanently unlocked. Launching...");
                    InvalidateRect(g_hStatusText, NULL, TRUE);
                    UpdateWindow(hwnd);
                    Sleep(600);
                    DestroyWindow(hwnd);
                } else {
                    g_statusColor = RGB(255, 80, 80);
                    SetWindowTextW(g_hStatusText, L"❌ Invalid activation key! Please send your code to @PcPortOP_bot.");
                    InvalidateRect(g_hStatusText, NULL, TRUE);
                }
            }
            return 0;
        }

        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            if (g_hBgBrush) DeleteObject(g_hBgBrush);
            if (g_hCardBrush) DeleteObject(g_hCardBrush);
            if (g_hHeaderBrush) DeleteObject(g_hHeaderBrush);
            if (g_hInputBrush) DeleteObject(g_hInputBrush);
            if (g_hFontTitle) DeleteObject(g_hFontTitle);
            if (g_hFontHeading) DeleteObject(g_hFontHeading);
            if (g_hFontSub) DeleteObject(g_hFontSub);
            if (g_hFontNormal) DeleteObject(g_hFontNormal);
            if (g_hFontMono) DeleteObject(g_hFontMono);
            if (g_hFontSmall) DeleteObject(g_hFontSmall);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int license_check_prompt(void) {
    const char* path = GetLicenseFilePath();
    strncpy(g_license_path, path, sizeof(g_license_path) - 1);
    if (IsActivated(g_license_path, g_hwid)) {
        return 1; // Already activated! Instant direct boot.
    }

    WNDCLASSW wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hbrBackground = NULL;
    wc.lpszClassName = L"PCPortOP_Activation_Modern_Wnd";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassW(&wc);

    int screen_w = GetSystemMetrics(SM_CXSCREEN);
    int screen_h = GetSystemMetrics(SM_CYSCREEN);
    int win_w = 560;
    int win_h = 485;
    int pos_x = (screen_w - win_w) / 2;
    int pos_y = (screen_h - win_h) / 2;

    HWND hwnd = CreateWindowW(
        L"PCPortOP_Activation_Modern_Wnd",
        L"PCPortOP — God of War III Remastered (V3)",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        pos_x, pos_y, win_w, win_h,
        NULL, NULL, GetModuleHandle(NULL), NULL
    );

    if (!hwnd) return 0;

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return g_activated;
}