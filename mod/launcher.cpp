#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <shellapi.h>
#include <winhttp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <string>
#include <vector>

#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "winhttp.lib")

#define IDC_EDIT_GAMEDIR    101
#define IDC_BTN_BROWSE      102
#define IDC_EDIT_RELAYURL   103
#define IDC_EDIT_PLAYERNAME 104
#define IDC_RADIO_1V1       105
#define IDC_RADIO_2V2       106
#define IDC_COMBO_SLOT      107
#define IDC_COMBO_BALLS     108
#define IDC_EDIT_ROOMCODE   109
#define IDC_BTN_NEWCODE     110
#define IDC_BTN_COPYCODE    111
#define IDC_EDIT_LBCODE     112
#define IDC_BTN_CREATELB    113
#define IDC_BTN_OPENLB      114
#define IDC_BTN_PLAY_ONLINE 115
#define IDC_BTN_PLAY_OFF    116
#define IDC_BTN_UNINSTALL   117
#define IDC_STATIC_STATUS   118

#define WM_APP_WAKE_DONE     (WM_USER + 1)
#define WM_APP_CREATELB_DONE (WM_USER + 2)

static const char* kDefaultRelayUrl = "https://peggle-online-relay.onrender.com";

static int   g_Dpi = 96;
static HWND  g_HwndMain = nullptr;
static HWND  g_EditGameDir = nullptr;
static HWND  g_BtnBrowse = nullptr;
static HWND  g_EditRelayUrl = nullptr;
static HWND  g_EditPlayerName = nullptr;
static HWND  g_Radio1v1 = nullptr;
static HWND  g_Radio2v2 = nullptr;
static HWND  g_ComboSlot = nullptr;
static HWND  g_ComboBalls = nullptr;
static HWND  g_EditRoomCode = nullptr;
static HWND  g_BtnNewCode = nullptr;
static HWND  g_BtnCopyCode = nullptr;
static HWND  g_EditLbCode = nullptr;
static HWND  g_BtnCreateLb = nullptr;
static HWND  g_BtnOpenLb = nullptr;
static HWND  g_BtnPlayOnline = nullptr;
static HWND  g_BtnPlayOffline = nullptr;
static HWND  g_BtnUninstall = nullptr;
static HWND  g_StaticStatus = nullptr;

static HFONT g_FontNormal = nullptr;
static HFONT g_FontBold = nullptr;
static HFONT g_FontCode = nullptr;
static HFONT g_FontTitle = nullptr;

static char  g_LauncherDir[MAX_PATH] = {};
static char  g_RelayConfigIni[MAX_PATH] = {};
static bool  g_Busy = false;

static int S(int px) {
    return MulDiv(px, g_Dpi, 96);
}

static bool FileExistsA(const char* path) {
    DWORD attr = GetFileAttributesA(path);
    return (attr != INVALID_FILE_ATTRIBUTES) && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

static bool DirExistsA(const char* path) {
    DWORD attr = GetFileAttributesA(path);
    return (attr != INVALID_FILE_ATTRIBUTES) && (attr & FILE_ATTRIBUTE_DIRECTORY);
}

static bool IsValidGameDir(const char* dir) {
    if (!dir || !dir[0] || !DirExistsA(dir)) return false;
    char exePath[MAX_PATH];
    snprintf(exePath, sizeof(exePath), "%s\\PeggleNights.exe", dir);
    return FileExistsA(exePath);
}

static void TrimTrailingSlash(char* s) {
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\\' || s[n - 1] == '/')) {
        s[--n] = '\0';
    }
}

static void NormalizeSlashes(char* s) {
    for (size_t i = 0; s[i]; i++) {
        if (s[i] == '/') s[i] = '\\';
    }
}

static bool CaseInsensitiveContains(const char* haystack, const char* needle) {
    if (!haystack || !needle) return false;
    std::string h(haystack);
    std::string n(needle);
    for (char& c : h) c = (char)tolower((unsigned char)c);
    for (char& c : n) c = (char)tolower((unsigned char)c);
    return h.find(n) != std::string::npos;
}

static void ParseLibraryFoldersVdf(const char* vdfPath, std::vector<std::string>& outLibs) {
    FILE* f = fopen(vdfPath, "rb");
    if (!f) return;
    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        const char* key = strstr(line, "\"path\"");
        if (!key) continue;
        const char* q1 = strchr(key + 6, '"');
        if (!q1) continue;
        const char* q2 = strchr(q1 + 1, '"');
        if (!q2) continue;

        std::string raw(q1 + 1, q2);
        std::string unescaped;
        for (size_t i = 0; i < raw.size(); i++) {
            if (raw[i] == '\\' && i + 1 < raw.size() && raw[i + 1] == '\\') {
                unescaped.push_back('\\');
                i++;
            } else {
                unescaped.push_back(raw[i]);
            }
        }
        if (!unescaped.empty()) {
            outLibs.push_back(unescaped);
        }
    }
    fclose(f);
}

static bool DetectPeggleNightsDir(char* outDir, size_t outCap) {
    char savedDir[MAX_PATH] = {};
    GetPrivateProfileStringA("Relay", "game_dir", "", savedDir, sizeof(savedDir), g_RelayConfigIni);
    TrimTrailingSlash(savedDir);
    if (IsValidGameDir(savedDir)) {
        snprintf(outDir, outCap, "%s", savedDir);
        return true;
    }

    char selfDir[MAX_PATH];
    snprintf(selfDir, sizeof(selfDir), "%s", g_LauncherDir);
    TrimTrailingSlash(selfDir);
    if (IsValidGameDir(selfDir)) {
        snprintf(outDir, outCap, "%s", selfDir);
        return true;
    }

    HKEY hKey = nullptr;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Valve\\Steam", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        char steamPath[MAX_PATH] = {};
        DWORD sz = sizeof(steamPath);
        DWORD type = 0;
        if (RegQueryValueExA(hKey, "SteamPath", nullptr, &type, (LPBYTE)steamPath, &sz) == ERROR_SUCCESS &&
            (type == REG_SZ || type == REG_EXPAND_SZ)) {
            NormalizeSlashes(steamPath);
            TrimTrailingSlash(steamPath);

            std::vector<std::string> steamLibs;
            steamLibs.push_back(steamPath);

            char vdfPath[MAX_PATH];
            snprintf(vdfPath, sizeof(vdfPath), "%s\\steamapps\\libraryfolders.vdf", steamPath);
            ParseLibraryFoldersVdf(vdfPath, steamLibs);

            for (const std::string& lib : steamLibs) {
                char candidate[MAX_PATH];
                snprintf(candidate, sizeof(candidate), "%s\\steamapps\\common\\Peggle Nights", lib.c_str());
                if (IsValidGameDir(candidate)) {
                    RegCloseKey(hKey);
                    snprintf(outDir, outCap, "%s", candidate);
                    return true;
                }
            }
        }
        RegCloseKey(hKey);
    }

    const char* fallbacks[] = {
        "C:\\Program Files (x86)\\PopCap Games\\Peggle Nights",
        "C:\\Program Files\\PopCap Games\\Peggle Nights",
        "C:\\Program Files (x86)\\Steam\\steamapps\\common\\Peggle Nights"
    };
    for (const char* fb : fallbacks) {
        if (IsValidGameDir(fb)) {
            snprintf(outDir, outCap, "%s", fb);
            return true;
        }
    }

    outDir[0] = '\0';
    return false;
}

static bool ContainsBytes(const std::vector<uint8_t>& hay, const char* needle) {
    size_t nlen = strlen(needle);
    if (nlen == 0 || hay.size() < nlen) return false;
    for (size_t i = 0; i <= hay.size() - nlen; i++) {
        if (memcmp(hay.data() + i, needle, nlen) == 0) return true;
    }
    return false;
}

static bool IsFileOurModDll(const char* dllPath) {
    FILE* f = fopen(dllPath, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > 8 * 1024 * 1024) {
        fclose(f);
        return false;
    }
    std::vector<uint8_t> data((size_t)sz);
    size_t rd = fread(data.data(), 1, (size_t)sz, f);
    fclose(f);
    data.resize(rd);
    return ContainsBytes(data, "PEGGLE_ONLINE_MOD_V2") || ContainsBytes(data, "PeggleOnline Mod");
}

static bool InstallModToGameDir(const char* gameDir, char* errBuf, size_t errCap) {
    char srcDll[MAX_PATH];
    snprintf(srcDll, sizeof(srcDll), "%sbass.dll", g_LauncherDir);
    if (!FileExistsA(srcDll)) {
        snprintf(errBuf, errCap, "Missing bass.dll next to PlayPeggleOnline.exe!\nMake sure you extracted the entire zip folder.");
        return false;
    }
    if (!IsValidGameDir(gameDir)) {
        snprintf(errBuf, errCap, "PeggleNights.exe was not found in:\n%s", gameDir);
        return false;
    }

    char dstDll[MAX_PATH];
    char origDll[MAX_PATH];
    snprintf(dstDll, sizeof(dstDll), "%s\\bass.dll", gameDir);
    snprintf(origDll, sizeof(origDll), "%s\\bass_orig.dll", gameDir);

    if (!FileExistsA(origDll)) {
        if (!FileExistsA(dstDll)) {
            snprintf(errBuf, errCap, "Original bass.dll was not found in:\n%s", gameDir);
            return false;
        }
        if (IsFileOurModDll(dstDll)) {
            snprintf(errBuf, errCap,
                "bass.dll in the game folder is already modded, and bass_orig.dll backup is missing.\n"
                "Please verify game files in Steam (or restore original bass.dll) first.");
            return false;
        }
        if (!CopyFileA(dstDll, origDll, TRUE)) {
            DWORD e = GetLastError();
            snprintf(errBuf, errCap, "Failed to back up bass.dll -> bass_orig.dll (error %u).", e);
            return false;
        }
    }

    if (_stricmp(srcDll, dstDll) != 0) {
        if (!CopyFileA(srcDll, dstDll, FALSE)) {
            DWORD e = GetLastError();
            if (e == ERROR_SHARING_VIOLATION || e == ERROR_ACCESS_DENIED) {
                snprintf(errBuf, errCap, "Please close Peggle Nights first and try again.");
            } else {
                snprintf(errBuf, errCap, "Failed to install bass.dll (Windows error %u).", e);
            }
            return false;
        }
    }
    return true;
}

static int GetSelectedSlot() {
    int sel = (int)SendMessageA(g_ComboSlot, CB_GETCURSEL, 0, 0);
    return (sel < 0) ? 0 : sel;
}

static bool Is2v2Selected() {
    return (SendMessageA(g_Radio2v2, BM_GETCHECK, 0, 0) == BST_CHECKED);
}

static int GetSelectedBallsPerPlayer() {
    int idx = (int)SendMessageA(g_ComboBalls, CB_GETCURSEL, 0, 0);
    static const int kBalls[4] = { 3, 4, 5, 6 };
    if (idx < 0 || idx > 3) return 4;
    return kBalls[idx];
}

static void PopulateSlotCombo(bool is2v2, int keepSlot) {
    SendMessageA(g_ComboSlot, CB_RESETCONTENT, 0, 0);
    if (is2v2) {
        SendMessageA(g_ComboSlot, CB_ADDSTRING, 0, (LPARAM)"Team A - Player 1 (Host)");
        SendMessageA(g_ComboSlot, CB_ADDSTRING, 0, (LPARAM)"Team B - Player 2");
        SendMessageA(g_ComboSlot, CB_ADDSTRING, 0, (LPARAM)"Team A - Player 3");
        SendMessageA(g_ComboSlot, CB_ADDSTRING, 0, (LPARAM)"Team B - Player 4");
        if (keepSlot < 0 || keepSlot > 3) keepSlot = 0;
    } else {
        SendMessageA(g_ComboSlot, CB_ADDSTRING, 0, (LPARAM)"Player 1 (Host)");
        SendMessageA(g_ComboSlot, CB_ADDSTRING, 0, (LPARAM)"Player 2 (Join)");
        if (keepSlot < 0 || keepSlot > 1) keepSlot = 0;
    }
    SendMessageA(g_ComboSlot, CB_SETCURSEL, (WPARAM)keepSlot, 0);
    EnableWindow(g_ComboBalls, !g_Busy && keepSlot == 0);
}

static void SetStatusText(const char* text) {
    SetWindowTextA(g_StaticStatus, text);
}

static void SetUiEnabled(bool enabled) {
    EnableWindow(g_EditGameDir, enabled);
    EnableWindow(g_BtnBrowse, enabled);
    EnableWindow(g_EditRelayUrl, enabled);
    EnableWindow(g_EditPlayerName, enabled);
    EnableWindow(g_Radio1v1, enabled);
    EnableWindow(g_Radio2v2, enabled);
    EnableWindow(g_ComboSlot, enabled);
    EnableWindow(g_ComboBalls, enabled && GetSelectedSlot() == 0);
    EnableWindow(g_EditRoomCode, enabled);
    EnableWindow(g_BtnNewCode, enabled);
    EnableWindow(g_BtnCopyCode, enabled);
    EnableWindow(g_EditLbCode, enabled);
    EnableWindow(g_BtnCreateLb, enabled);
    EnableWindow(g_BtnOpenLb, enabled);
    EnableWindow(g_BtnPlayOnline, enabled);
    EnableWindow(g_BtnPlayOffline, enabled);
    EnableWindow(g_BtnUninstall, enabled);
}

static void GenerateRandomRoomCode(char* out, size_t cap) {
    static const char kAlphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
    static bool seeded = false;
    if (!seeded) {
        srand(GetTickCount() ^ GetCurrentProcessId());
        seeded = true;
    }
    if (cap < 5) return;
    for (int i = 0; i < 4; i++) {
        out[i] = kAlphabet[rand() % (sizeof(kAlphabet) - 1)];
    }
    out[4] = '\0';
}

static void SanitizeRoomCode(const char* raw, char* out, size_t cap) {
    size_t w = 0;
    for (size_t i = 0; raw[i] && w + 1 < cap && w < 12; i++) {
        unsigned char c = (unsigned char)raw[i];
        if (isalnum(c)) out[w++] = (char)toupper(c);
    }
    out[w] = '\0';
}

static void SanitizeLbCode(const char* raw, char* out, size_t cap) {
    size_t w = 0;
    for (size_t i = 0; raw[i] && w + 1 < cap && w < 16; i++) {
        unsigned char c = (unsigned char)raw[i];
        if (isalnum(c) || c == '-') out[w++] = (char)toupper(c);
    }
    out[w] = '\0';
}

static void SanitizeDisplayName(const char* raw, char* out, size_t cap) {
    size_t w = 0;
    for (size_t i = 0; raw[i] && w + 1 < cap && w < 24; i++) {
        unsigned char c = (unsigned char)raw[i];
        if (c >= 0x20 && c != 0x7f && c != '"' && c != '\\') {
            out[w++] = (char)c;
        }
    }
    out[w] = '\0';
    while (w > 0 && isspace((unsigned char)out[w - 1])) out[--w] = '\0';
    if (w == 0) snprintf(out, cap, "Player");
}

static void TrimWhitespace(char* s) {
    size_t len = strlen(s);
    size_t start = 0;
    while (start < len && isspace((unsigned char)s[start])) start++;
    size_t end = len;
    while (end > start && isspace((unsigned char)s[end - 1])) end--;
    if (start > 0) memmove(s, s + start, end - start);
    s[end - start] = '\0';
}

static void CopyTextToClipboard(const char* text) {
    if (!text || !text[0]) return;
    if (OpenClipboard(g_HwndMain)) {
        EmptyClipboard();
        size_t len = strlen(text) + 1;
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
        if (hMem) {
            void* ptr = GlobalLock(hMem);
            memcpy(ptr, text, len);
            GlobalUnlock(hMem);
            SetClipboardData(CF_TEXT, hMem);
        }
        CloseClipboard();
    }
}

static bool WriteGameIni(
    const char* gameDir,
    bool enabled,
    int modePlayers,
    int playerSlot,
    int ballsPerPlayer,
    const char* playerName,
    const char* lbCode,
    const char* relayUrl,
    const char* roomCode
) {
    char iniPath[MAX_PATH];
    snprintf(iniPath, sizeof(iniPath), "%s\\peggle_online.ini", gameDir);
    FILE* f = fopen(iniPath, "w");
    if (!f) return false;
    fprintf(f, "[Online]\n");
    fprintf(f, "enabled = %d\n", enabled ? 1 : 0);
    fprintf(f, "role = %s\n", (playerSlot == 0) ? "ws_host" : "ws_client");
    fprintf(f, "mode_players = %d\n", modePlayers);
    fprintf(f, "player_slot = %d\n", playerSlot);
    fprintf(f, "balls_per_player = %d\n", ballsPerPlayer);
    fprintf(f, "player_name = %s\n", playerName);
    fprintf(f, "leaderboard_code = %s\n", lbCode);
    fprintf(f, "relay_url = %s\n", relayUrl);
    fprintf(f, "room_code = %s\n", roomCode);
    fprintf(f, "host_ip = 127.0.0.1\n");
    fprintf(f, "port = 27888\n");
    fclose(f);
    return true;
}

static void SaveRelayConfigIni(
    const char* relayUrl,
    const char* gameDir,
    const char* playerName,
    const char* lbCode,
    int modePlayers,
    int ballsPerPlayer
) {
    char numBuf[16];
    WritePrivateProfileStringA("Relay", "relay_url", relayUrl, g_RelayConfigIni);
    WritePrivateProfileStringA("Relay", "game_dir", gameDir, g_RelayConfigIni);
    WritePrivateProfileStringA("Relay", "player_name", playerName, g_RelayConfigIni);
    WritePrivateProfileStringA("Relay", "leaderboard_code", lbCode, g_RelayConfigIni);
    snprintf(numBuf, sizeof(numBuf), "%d", modePlayers);
    WritePrivateProfileStringA("Relay", "mode_players", numBuf, g_RelayConfigIni);
    snprintf(numBuf, sizeof(numBuf), "%d", ballsPerPlayer);
    WritePrivateProfileStringA("Relay", "balls_per_player", numBuf, g_RelayConfigIni);
}

static void LaunchPeggleGame(const char* gameDir) {
    if (CaseInsensitiveContains(gameDir, "steamapps\\common\\Peggle Nights")) {
        ShellExecuteA(nullptr, "open", "steam://rungameid/3540", nullptr, nullptr, SW_SHOWNORMAL);
    } else {
        char exePath[MAX_PATH];
        snprintf(exePath, sizeof(exePath), "%s\\PeggleNights.exe", gameDir);
        ShellExecuteA(nullptr, "open", exePath, nullptr, gameDir, SW_SHOWNORMAL);
    }
}

static bool ParseRelayHttpTarget(
    const char* rawUrl,
    wchar_t* outHost,
    DWORD outHostCap,
    INTERNET_PORT* outPort,
    bool* outSecure,
    wchar_t* outBasePath,
    DWORD outBasePathCap,
    std::string* outNormalizedBase
) {
    char normalized[320] = {};
    if (_strnicmp(rawUrl, "wss://", 6) == 0) {
        snprintf(normalized, sizeof(normalized), "https://%s", rawUrl + 6);
    } else if (_strnicmp(rawUrl, "ws://", 5) == 0) {
        snprintf(normalized, sizeof(normalized), "http://%s", rawUrl + 5);
    } else if (_strnicmp(rawUrl, "https://", 8) == 0 || _strnicmp(rawUrl, "http://", 7) == 0) {
        snprintf(normalized, sizeof(normalized), "%s", rawUrl);
    } else {
        snprintf(normalized, sizeof(normalized), "https://%s", rawUrl);
    }

    size_t nlen = strlen(normalized);
    while (nlen > 0 && normalized[nlen - 1] == '/') normalized[--nlen] = '\0';
    if (outNormalizedBase) *outNormalizedBase = normalized;

    wchar_t wideUrl[320] = {};
    MultiByteToWideChar(CP_UTF8, 0, normalized, -1, wideUrl, 320);

    wchar_t pathBuf[256] = {};
    URL_COMPONENTSW uc = {};
    uc.dwStructSize = sizeof(uc);
    uc.lpszHostName = outHost;
    uc.dwHostNameLength = outHostCap;
    uc.lpszUrlPath = pathBuf;
    uc.dwUrlPathLength = 256;

    if (!WinHttpCrackUrl(wideUrl, 0, 0, &uc) || outHost[0] == L'\0') return false;

    *outPort = uc.nPort;
    *outSecure = (uc.nScheme == INTERNET_SCHEME_HTTPS);
    size_t plen = wcslen(pathBuf);
    while (plen > 0 && pathBuf[plen - 1] == L'/') pathBuf[--plen] = L'\0';
    wcsncpy(outBasePath, pathBuf, outBasePathCap - 1);
    outBasePath[outBasePathCap - 1] = L'\0';
    return true;
}

static bool HttpRequestRelay(
    const char* rawUrl,
    const wchar_t* method,
    const wchar_t* subPath,
    const char* jsonBody,
    DWORD* outStatusOrErr,
    std::string* outRespBody
) {
    wchar_t hostName[160] = {};
    wchar_t basePath[160] = {};
    INTERNET_PORT port = INTERNET_DEFAULT_HTTPS_PORT;
    bool isSecure = true;

    if (!ParseRelayHttpTarget(rawUrl, hostName, 160, &port, &isSecure, basePath, 160, nullptr)) {
        *outStatusOrErr = ERROR_INVALID_PARAMETER;
        return false;
    }

    wchar_t fullPath[256] = {};
    _snwprintf(fullPath, 256, L"%s%s", basePath, subPath);
    fullPath[255] = L'\0';

    HINTERNET hSession = WinHttpOpen(
        L"PeggleOnlineLauncher/2.0",
        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
    );
    if (!hSession) { *outStatusOrErr = GetLastError(); return false; }
    WinHttpSetTimeouts(hSession, 10000, 15000, 15000, 45000);

    HINTERNET hConnect = WinHttpConnect(hSession, hostName, port, 0);
    if (!hConnect) { *outStatusOrErr = GetLastError(); WinHttpCloseHandle(hSession); return false; }

    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect, method, fullPath, nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, isSecure ? WINHTTP_FLAG_SECURE : 0
    );
    if (!hRequest) {
        *outStatusOrErr = GetLastError();
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    const wchar_t* hdrs = jsonBody ? L"Content-Type: application/json\r\n" : WINHTTP_NO_ADDITIONAL_HEADERS;
    DWORD bodyLen = jsonBody ? (DWORD)strlen(jsonBody) : 0;

    bool ok = false;
    if (WinHttpSendRequest(hRequest, hdrs, jsonBody ? (DWORD)-1 : 0, (LPVOID)jsonBody, bodyLen, bodyLen, 0) &&
        WinHttpReceiveResponse(hRequest, nullptr)) {
        DWORD status = 0, statusSz = sizeof(status);
        if (WinHttpQueryHeaders(
                hRequest,
                WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                WINHTTP_HEADER_NAME_BY_INDEX,
                &status,
                &statusSz,
                WINHTTP_NO_HEADER_INDEX)) {
            *outStatusOrErr = status;
            ok = (status >= 200 && status < 300);
        }
        if (outRespBody) {
            DWORD avail = 0;
            while (WinHttpQueryDataAvailable(hRequest, &avail) && avail > 0) {
                std::vector<char> buf(avail);
                DWORD read = 0;
                if (WinHttpReadData(hRequest, buf.data(), avail, &read) && read > 0) {
                    outRespBody->append(buf.data(), read);
                } else break;
            }
        }
    } else {
        *outStatusOrErr = GetLastError();
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return ok;
}

struct WakeThreadArgs {
    std::string relayUrl;
};

static DWORD WINAPI WakeServerThread(LPVOID param) {
    WakeThreadArgs* args = (WakeThreadArgs*)param;
    DWORD code = 0;
    bool ok = HttpRequestRelay(args->relayUrl.c_str(), L"GET", L"/health", nullptr, &code, nullptr);
    delete args;
    PostMessageA(g_HwndMain, WM_APP_WAKE_DONE, ok ? 1 : 0, (LPARAM)code);
    return 0;
}

struct CreateLbArgs {
    std::string relayUrl;
    std::string lbName;
    std::string lbCode;
};

struct CreateLbResult {
    bool ok = false;
    DWORD status = 0;
    std::string code;
    std::string baseUrl;
};

static std::string ExtractJsonStringField(const std::string& json, const char* field) {
    std::string pat = std::string("\"") + field + "\":\"";
    size_t pos = json.find(pat);
    if (pos == std::string::npos) return "";
    size_t start = pos + pat.size();
    size_t end = json.find('"', start);
    if (end == std::string::npos) return "";
    return json.substr(start, end - start);
}

static DWORD WINAPI CreateLbThread(LPVOID param) {
    CreateLbArgs* args = (CreateLbArgs*)param;
    char body[512];
    if (args->lbCode.empty()) {
        snprintf(body, sizeof(body), "{\"name\":\"%s\"}", args->lbName.c_str());
    } else {
        snprintf(body, sizeof(body), "{\"name\":\"%s\",\"code\":\"%s\",\"getOrCreate\":true}",
                 args->lbName.c_str(), args->lbCode.c_str());
    }

    CreateLbResult* res = new CreateLbResult();
    std::string resp;
    res->ok = HttpRequestRelay(args->relayUrl.c_str(), L"POST", L"/api/leaderboards", body, &res->status, &resp);
    if (res->ok) {
        res->code = ExtractJsonStringField(resp, "code");
    }
    wchar_t h[160] = {}, bp[160] = {};
    INTERNET_PORT port = 443;
    bool sec = true;
    ParseRelayHttpTarget(args->relayUrl.c_str(), h, 160, &port, &sec, bp, 160, &res->baseUrl);
    delete args;
    PostMessageA(g_HwndMain, WM_APP_CREATELB_DONE, 0, (LPARAM)res);
    return 0;
}

static int CALLBACK BrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM /*lParam*/, LPARAM lpData) {
    if (uMsg == BFFM_INITIALIZED && lpData) {
        const char* initDir = (const char*)lpData;
        if (initDir[0] && DirExistsA(initDir)) {
            SendMessageA(hwnd, BFFM_SETSELECTIONA, TRUE, (LPARAM)initDir);
        }
    }
    return 0;
}

static void OnBrowseGameDir() {
    char currentDir[MAX_PATH] = {};
    GetWindowTextA(g_EditGameDir, currentDir, sizeof(currentDir));
    TrimTrailingSlash(currentDir);

    BROWSEINFOA bi = {};
    bi.hwndOwner = g_HwndMain;
    bi.lpszTitle = "Select the folder containing PeggleNights.exe";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    bi.lpfn = BrowseCallbackProc;
    bi.lParam = (LPARAM)currentDir;

    PIDLIST_ABSOLUTE pidl = SHBrowseForFolderA(&bi);
    if (pidl) {
        char chosen[MAX_PATH] = {};
        if (SHGetPathFromIDListA(pidl, chosen)) {
            TrimTrailingSlash(chosen);
            if (IsValidGameDir(chosen)) {
                SetWindowTextA(g_EditGameDir, chosen);
                SetStatusText("Game folder updated! Ready to play.");
            } else {
                MessageBoxA(
                    g_HwndMain,
                    "PeggleNights.exe was not found in that folder.\nPlease choose the folder containing PeggleNights.exe.",
                    "Peggle Nights Not Found",
                    MB_ICONWARNING
                );
            }
        }
        CoTaskMemFree(pidl);
    }
}

static void OnCopyRoomCode() {
    char raw[32] = {}, clean[32] = {};
    GetWindowTextA(g_EditRoomCode, raw, sizeof(raw));
    SanitizeRoomCode(raw, clean, sizeof(clean));
    if (!clean[0]) {
        SetStatusText("Enter a room code first.");
        return;
    }
    SetWindowTextA(g_EditRoomCode, clean);
    CopyTextToClipboard(clean);
    char msg[128];
    snprintf(msg, sizeof(msg), "Room code %s copied! Send it to your friends.", clean);
    SetStatusText(msg);
}

static void OnCreateLeaderboard() {
    if (g_Busy) return;
    char relayUrl[256] = {};
    GetWindowTextA(g_EditRelayUrl, relayUrl, sizeof(relayUrl));
    TrimWhitespace(relayUrl);
    if (!relayUrl[0]) {
        MessageBoxA(g_HwndMain, "Please enter your Render relay server URL first.", "Relay URL Missing", MB_ICONWARNING);
        return;
    }

    char rawName[48] = {}, cleanName[48] = {};
    GetWindowTextA(g_EditPlayerName, rawName, sizeof(rawName));
    SanitizeDisplayName(rawName, cleanName, sizeof(cleanName));

    char rawLb[32] = {}, cleanLb[32] = {};
    GetWindowTextA(g_EditLbCode, rawLb, sizeof(rawLb));
    SanitizeLbCode(rawLb, cleanLb, sizeof(cleanLb));

    CreateLbArgs* args = new CreateLbArgs();
    args->relayUrl = relayUrl;
    args->lbCode = cleanLb;
    if (cleanLb[0]) {
        args->lbName = cleanLb;
    } else {
        args->lbName = std::string(cleanName) + " Club";
    }

    g_Busy = true;
    SetUiEnabled(false);
    SetStatusText("Creating leaderboard on Render server...");
    HANDLE hThread = CreateThread(nullptr, 0, CreateLbThread, args, 0, nullptr);
    if (hThread) CloseHandle(hThread);
    else { delete args; g_Busy = false; SetUiEnabled(true); }
}

static void OnOpenLeaderboardWeb() {
    char relayUrl[256] = {};
    GetWindowTextA(g_EditRelayUrl, relayUrl, sizeof(relayUrl));
    TrimWhitespace(relayUrl);
    if (!relayUrl[0]) return;

    wchar_t h[160] = {}, bp[160] = {};
    INTERNET_PORT port = 443;
    bool sec = true;
    std::string normBase;
    if (!ParseRelayHttpTarget(relayUrl, h, 160, &port, &sec, bp, 160, &normBase)) {
        MessageBoxA(g_HwndMain, "Please enter a valid Relay Server URL.", "Invalid URL", MB_ICONWARNING);
        return;
    }

    char rawLb[32] = {}, cleanLb[32] = {};
    GetWindowTextA(g_EditLbCode, rawLb, sizeof(rawLb));
    SanitizeLbCode(rawLb, cleanLb, sizeof(cleanLb));

    std::string targetUrl = normBase + "/leaderboard";
    if (cleanLb[0]) {
        targetUrl += "/" + std::string(cleanLb);
    }
    ShellExecuteA(nullptr, "open", targetUrl.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

static std::string g_PendingLaunchDir;

static void OnPlayOnline() {
    if (g_Busy) return;

    char gameDir[MAX_PATH] = {};
    GetWindowTextA(g_EditGameDir, gameDir, sizeof(gameDir));
    TrimWhitespace(gameDir);
    TrimTrailingSlash(gameDir);
    if (!IsValidGameDir(gameDir)) {
        MessageBoxA(
            g_HwndMain,
            "Please select a valid Peggle Nights folder (containing PeggleNights.exe).",
            "Game Folder Missing",
            MB_ICONWARNING
        );
        return;
    }

    char relayUrl[256] = {};
    GetWindowTextA(g_EditRelayUrl, relayUrl, sizeof(relayUrl));
    TrimWhitespace(relayUrl);
    if (!relayUrl[0]) {
        MessageBoxA(g_HwndMain, "Please enter your Render relay server URL.", "Relay URL Missing", MB_ICONWARNING);
        return;
    }

    char rawName[48] = {}, playerName[48] = {};
    GetWindowTextA(g_EditPlayerName, rawName, sizeof(rawName));
    SanitizeDisplayName(rawName, playerName, sizeof(playerName));
    SetWindowTextA(g_EditPlayerName, playerName);

    char rawCode[32] = {}, roomCode[32] = {};
    GetWindowTextA(g_EditRoomCode, rawCode, sizeof(rawCode));
    SanitizeRoomCode(rawCode, roomCode, sizeof(roomCode));
    if (strlen(roomCode) < 2) {
        MessageBoxA(
            g_HwndMain,
            "Room code must be 2 to 12 letters/numbers (for example: PEGG).",
            "Invalid Room Code",
            MB_ICONWARNING
        );
        return;
    }
    SetWindowTextA(g_EditRoomCode, roomCode);

    char rawLb[32] = {}, lbCode[32] = {};
    GetWindowTextA(g_EditLbCode, rawLb, sizeof(rawLb));
    SanitizeLbCode(rawLb, lbCode, sizeof(lbCode));
    SetWindowTextA(g_EditLbCode, lbCode);

    int modePlayers = Is2v2Selected() ? 4 : 2;
    int playerSlot = GetSelectedSlot();
    int ballsPerPlayer = GetSelectedBallsPerPlayer();

    char errBuf[512] = {};
    if (!InstallModToGameDir(gameDir, errBuf, sizeof(errBuf))) {
        MessageBoxA(g_HwndMain, errBuf, "Mod Install Error", MB_ICONERROR);
        return;
    }

    SaveRelayConfigIni(relayUrl, gameDir, playerName, lbCode, modePlayers, ballsPerPlayer);
    if (!WriteGameIni(gameDir, true, modePlayers, playerSlot, ballsPerPlayer, playerName, lbCode, relayUrl, roomCode)) {
        MessageBoxA(
            g_HwndMain,
            "Failed to write peggle_online.ini in the game folder.\nTry running as Administrator if the game is in Program Files.",
            "Config Write Error",
            MB_ICONERROR
        );
        return;
    }

    g_Busy = true;
    g_PendingLaunchDir = gameDir;
    SetUiEnabled(false);
    SetStatusText("Waking up Render server (first load can take ~20s)...");

    WakeThreadArgs* args = new WakeThreadArgs();
    args->relayUrl = relayUrl;
    HANDLE hThread = CreateThread(nullptr, 0, WakeServerThread, args, 0, nullptr);
    if (hThread) {
        CloseHandle(hThread);
    } else {
        delete args;
        g_Busy = false;
        SetUiEnabled(true);
        LaunchPeggleGame(gameDir);
    }
}

static void OnPlayOffline() {
    char gameDir[MAX_PATH] = {};
    GetWindowTextA(g_EditGameDir, gameDir, sizeof(gameDir));
    TrimWhitespace(gameDir);
    TrimTrailingSlash(gameDir);
    if (!IsValidGameDir(gameDir)) {
        MessageBoxA(g_HwndMain, "Please select a valid Peggle Nights folder first.", "Game Folder Missing", MB_ICONWARNING);
        return;
    }

    char dstDll[MAX_PATH];
    snprintf(dstDll, sizeof(dstDll), "%s\\bass.dll", gameDir);
    char iniPath[MAX_PATH];
    snprintf(iniPath, sizeof(iniPath), "%s\\peggle_online.ini", gameDir);
    if (FileExistsA(iniPath) || IsFileOurModDll(dstDll)) {
        char relayUrl[256] = {};
        GetWindowTextA(g_EditRelayUrl, relayUrl, sizeof(relayUrl));
        TrimWhitespace(relayUrl);
        WriteGameIni(gameDir, false, 2, 0, 6, "Player", "", relayUrl[0] ? relayUrl : kDefaultRelayUrl, "PEGG");
    }

    SetStatusText("Launching Peggle Nights in Offline mode...");
    LaunchPeggleGame(gameDir);
}

static void OnUninstallMod() {
    char gameDir[MAX_PATH] = {};
    GetWindowTextA(g_EditGameDir, gameDir, sizeof(gameDir));
    TrimWhitespace(gameDir);
    TrimTrailingSlash(gameDir);
    if (!IsValidGameDir(gameDir)) {
        MessageBoxA(g_HwndMain, "Please select a valid Peggle Nights folder first.", "Game Folder Missing", MB_ICONWARNING);
        return;
    }

    char dstDll[MAX_PATH], origDll[MAX_PATH], iniPath[MAX_PATH], logPath[MAX_PATH];
    snprintf(dstDll, sizeof(dstDll), "%s\\bass.dll", gameDir);
    snprintf(origDll, sizeof(origDll), "%s\\bass_orig.dll", gameDir);
    snprintf(iniPath, sizeof(iniPath), "%s\\peggle_online.ini", gameDir);
    snprintf(logPath, sizeof(logPath), "%s\\peggle_online.log", gameDir);

    if (!FileExistsA(origDll)) {
        MessageBoxA(g_HwndMain, "The online mod does not appear to be installed (no bass_orig.dll backup found).", "Uninstall Mod", MB_ICONINFORMATION);
        return;
    }

    if (!CopyFileA(origDll, dstDll, FALSE)) {
        DWORD e = GetLastError();
        if (e == ERROR_SHARING_VIOLATION || e == ERROR_ACCESS_DENIED) {
            MessageBoxA(g_HwndMain, "Please close Peggle Nights first and try again.", "Uninstall Error", MB_ICONERROR);
        } else {
            char msg[128];
            snprintf(msg, sizeof(msg), "Failed to restore bass.dll (error %u).", e);
            MessageBoxA(g_HwndMain, msg, "Uninstall Error", MB_ICONERROR);
        }
        return;
    }

    DeleteFileA(origDll);
    DeleteFileA(iniPath);
    DeleteFileA(logPath);
    SetStatusText("Mod uninstalled and original bass.dll restored.");
    MessageBoxA(g_HwndMain, "Peggle Online mod uninstalled! Original game files restored.", "Uninstall Complete", MB_ICONINFORMATION);
}

static void CreateUiControls(HWND hwnd) {
    HDC hdc = GetDC(hwnd);
    g_Dpi = GetDeviceCaps(hdc, LOGPIXELSX);
    ReleaseDC(hwnd, hdc);

    g_FontTitle = CreateFontA(-S(18), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    g_FontBold = CreateFontA(-S(13), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    g_FontNormal = CreateFontA(-S(13), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    g_FontCode = CreateFontA(-S(17), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        FIXED_PITCH | FF_MODERN, "Consolas");

    auto makeLabel = [&](const char* text, int x, int y, int w, int h, HFONT font, UINT id = 0) {
        HWND c = CreateWindowExA(0, "STATIC", text, WS_CHILD | WS_VISIBLE | SS_LEFT,
            S(x), S(y), S(w), S(h), hwnd, (HMENU)(UINT_PTR)id, nullptr, nullptr);
        SendMessageA(c, WM_SETFONT, (WPARAM)font, TRUE);
        return c;
    };

    makeLabel("Peggle Nights Online Duel", 18, 14, 464, 26, g_FontTitle);
    makeLabel("Auto-installs the mod & connects 1v1 or 2v2 with Room Codes + Leaderboards.", 18, 42, 464, 18, g_FontNormal);

    makeLabel("Peggle Nights Folder:", 18, 70, 200, 18, g_FontBold);
    g_EditGameDir = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_READONLY,
        S(18), S(90), S(366), S(24), hwnd, (HMENU)IDC_EDIT_GAMEDIR, nullptr, nullptr);
    SendMessageA(g_EditGameDir, WM_SETFONT, (WPARAM)g_FontNormal, TRUE);

    g_BtnBrowse = CreateWindowExA(0, "BUTTON", "Browse...",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        S(392), S(89), S(90), S(26), hwnd, (HMENU)IDC_BTN_BROWSE, nullptr, nullptr);
    SendMessageA(g_BtnBrowse, WM_SETFONT, (WPARAM)g_FontNormal, TRUE);

    makeLabel("Relay Server URL (Render):", 18, 124, 250, 18, g_FontBold);
    g_EditRelayUrl = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
        S(18), S(144), S(464), S(24), hwnd, (HMENU)IDC_EDIT_RELAYURL, nullptr, nullptr);
    SendMessageA(g_EditRelayUrl, WM_SETFONT, (WPARAM)g_FontNormal, TRUE);

    makeLabel("Your Name:", 18, 178, 150, 18, g_FontBold);
    g_EditPlayerName = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
        S(18), S(198), S(220), S(24), hwnd, (HMENU)IDC_EDIT_PLAYERNAME, nullptr, nullptr);
    SendMessageA(g_EditPlayerName, EM_LIMITTEXT, 24, 0);
    SendMessageA(g_EditPlayerName, WM_SETFONT, (WPARAM)g_FontNormal, TRUE);

    makeLabel("Mode:", 252, 178, 150, 18, g_FontBold);
    g_Radio1v1 = CreateWindowExA(0, "BUTTON", "1v1 (2 Players)",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTORADIOBUTTON | WS_GROUP,
        S(252), S(200), S(112), S(22), hwnd, (HMENU)IDC_RADIO_1V1, nullptr, nullptr);
    SendMessageA(g_Radio1v1, WM_SETFONT, (WPARAM)g_FontNormal, TRUE);

    g_Radio2v2 = CreateWindowExA(0, "BUTTON", "2v2 (4 Players)",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTORADIOBUTTON,
        S(368), S(200), S(114), S(22), hwnd, (HMENU)IDC_RADIO_2V2, nullptr, nullptr);
    SendMessageA(g_Radio2v2, WM_SETFONT, (WPARAM)g_FontNormal, TRUE);

    makeLabel("Your Role / Team Slot:", 18, 232, 200, 18, g_FontBold);
    g_ComboSlot = CreateWindowExA(0, "COMBOBOX", "",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
        S(18), S(252), S(220), S(140), hwnd, (HMENU)IDC_COMBO_SLOT, nullptr, nullptr);
    SendMessageA(g_ComboSlot, WM_SETFONT, (WPARAM)g_FontNormal, TRUE);

    makeLabel("Balls per Player (Host sets):", 252, 232, 220, 18, g_FontBold);
    g_ComboBalls = CreateWindowExA(0, "COMBOBOX", "",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
        S(252), S(252), S(230), S(140), hwnd, (HMENU)IDC_COMBO_BALLS, nullptr, nullptr);
    SendMessageA(g_ComboBalls, WM_SETFONT, (WPARAM)g_FontNormal, TRUE);
    SendMessageA(g_ComboBalls, CB_ADDSTRING, 0, (LPARAM)"3 Balls / Player (2v2: 6/team)");
    SendMessageA(g_ComboBalls, CB_ADDSTRING, 0, (LPARAM)"4 Balls / Player (Recommended)");
    SendMessageA(g_ComboBalls, CB_ADDSTRING, 0, (LPARAM)"5 Balls / Player (2v2: 10/team)");
    SendMessageA(g_ComboBalls, CB_ADDSTRING, 0, (LPARAM)"6 Balls / Player (Classic 1v1)");

    makeLabel("Room Code (share this 4-letter code with your friends):", 18, 288, 430, 18, g_FontBold);
    g_EditRoomCode = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_UPPERCASE | ES_CENTER,
        S(18), S(309), S(150), S(30), hwnd, (HMENU)IDC_EDIT_ROOMCODE, nullptr, nullptr);
    SendMessageA(g_EditRoomCode, EM_LIMITTEXT, 12, 0);
    SendMessageA(g_EditRoomCode, WM_SETFONT, (WPARAM)g_FontCode, TRUE);

    g_BtnNewCode = CreateWindowExA(0, "BUTTON", "New Code",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        S(178), S(309), S(100), S(30), hwnd, (HMENU)IDC_BTN_NEWCODE, nullptr, nullptr);
    SendMessageA(g_BtnNewCode, WM_SETFONT, (WPARAM)g_FontNormal, TRUE);

    g_BtnCopyCode = CreateWindowExA(0, "BUTTON", "Copy Code",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        S(286), S(309), S(100), S(30), hwnd, (HMENU)IDC_BTN_COPYCODE, nullptr, nullptr);
    SendMessageA(g_BtnCopyCode, WM_SETFONT, (WPARAM)g_FontNormal, TRUE);

    makeLabel("Custom Leaderboard Code (optional - tracks wins & scores online):", 18, 350, 464, 18, g_FontBold);
    g_EditLbCode = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_UPPERCASE | ES_AUTOHSCROLL,
        S(18), S(371), S(210), S(26), hwnd, (HMENU)IDC_EDIT_LBCODE, nullptr, nullptr);
    SendMessageA(g_EditLbCode, EM_LIMITTEXT, 16, 0);
    SendMessageA(g_EditLbCode, WM_SETFONT, (WPARAM)g_FontNormal, TRUE);

    g_BtnCreateLb = CreateWindowExA(0, "BUTTON", "Create New",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        S(236), S(371), S(118), S(26), hwnd, (HMENU)IDC_BTN_CREATELB, nullptr, nullptr);
    SendMessageA(g_BtnCreateLb, WM_SETFONT, (WPARAM)g_FontNormal, TRUE);

    g_BtnOpenLb = CreateWindowExA(0, "BUTTON", "Open Web",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        S(362), S(371), S(120), S(26), hwnd, (HMENU)IDC_BTN_OPENLB, nullptr, nullptr);
    SendMessageA(g_BtnOpenLb, WM_SETFONT, (WPARAM)g_FontNormal, TRUE);

    g_StaticStatus = makeLabel("Ready! Click 'Play Online Duel!' to install & launch.",
        18, 408, 464, 38, g_FontNormal, IDC_STATIC_STATUS);

    g_BtnPlayOnline = CreateWindowExA(0, "BUTTON", "Play Online Duel!",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
        S(18), S(454), S(206), S(38), hwnd, (HMENU)IDC_BTN_PLAY_ONLINE, nullptr, nullptr);
    SendMessageA(g_BtnPlayOnline, WM_SETFONT, (WPARAM)g_FontBold, TRUE);

    g_BtnPlayOffline = CreateWindowExA(0, "BUTTON", "Play Offline",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        S(234), S(454), S(120), S(38), hwnd, (HMENU)IDC_BTN_PLAY_OFF, nullptr, nullptr);
    SendMessageA(g_BtnPlayOffline, WM_SETFONT, (WPARAM)g_FontNormal, TRUE);

    g_BtnUninstall = CreateWindowExA(0, "BUTTON", "Uninstall Mod",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        S(362), S(454), S(120), S(38), hwnd, (HMENU)IDC_BTN_UNINSTALL, nullptr, nullptr);
    SendMessageA(g_BtnUninstall, WM_SETFONT, (WPARAM)g_FontNormal, TRUE);

    // Populate initial values from relay_config.ini
    char detectedDir[MAX_PATH] = {};
    if (DetectPeggleNightsDir(detectedDir, sizeof(detectedDir))) {
        SetWindowTextA(g_EditGameDir, detectedDir);
    } else {
        SetWindowTextA(g_EditGameDir, "Click Browse... to locate PeggleNights.exe");
        SetStatusText("Game folder not auto-detected. Click Browse... to pick Peggle Nights.");
    }

    char relayUrl[256] = {};
    GetPrivateProfileStringA("Relay", "relay_url", kDefaultRelayUrl, relayUrl, sizeof(relayUrl), g_RelayConfigIni);
    TrimWhitespace(relayUrl);
    SetWindowTextA(g_EditRelayUrl, relayUrl[0] ? relayUrl : kDefaultRelayUrl);

    char defaultUser[48] = "Player";
    DWORD unLen = sizeof(defaultUser);
    GetUserNameA(defaultUser, &unLen);
    char playerName[48] = {};
    GetPrivateProfileStringA("Relay", "player_name", defaultUser, playerName, sizeof(playerName), g_RelayConfigIni);
    SanitizeDisplayName(playerName, playerName, sizeof(playerName));
    SetWindowTextA(g_EditPlayerName, playerName);

    char lbCode[32] = {};
    GetPrivateProfileStringA("Relay", "leaderboard_code", "", lbCode, sizeof(lbCode), g_RelayConfigIni);
    SanitizeLbCode(lbCode, lbCode, sizeof(lbCode));
    SetWindowTextA(g_EditLbCode, lbCode);

    int savedMode = GetPrivateProfileIntA("Relay", "mode_players", 2, g_RelayConfigIni);
    bool is2v2 = (savedMode == 4);
    SendMessageA(is2v2 ? g_Radio2v2 : g_Radio1v1, BM_SETCHECK, BST_CHECKED, 0);
    PopulateSlotCombo(is2v2, 0);

    int savedBalls = GetPrivateProfileIntA("Relay", "balls_per_player", 4, g_RelayConfigIni);
    int ballIdx = 1;
    if (savedBalls == 3) ballIdx = 0;
    else if (savedBalls == 5) ballIdx = 2;
    else if (savedBalls == 6) ballIdx = 3;
    SendMessageA(g_ComboBalls, CB_SETCURSEL, (WPARAM)ballIdx, 0);

    char roomCode[16] = {};
    GenerateRandomRoomCode(roomCode, sizeof(roomCode));
    SetWindowTextA(g_EditRoomCode, roomCode);
}

static LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        g_HwndMain = hwnd;
        CreateUiControls(hwnd);
        return 0;

    case WM_CTLCOLORSTATIC: {
        HDC hdcStatic = (HDC)wParam;
        SetBkMode(hdcStatic, TRANSPARENT);
        return (LRESULT)GetSysColorBrush(COLOR_BTNFACE);
    }

    case WM_COMMAND: {
        int id = LOWORD(wParam);
        int code = HIWORD(wParam);
        if (id == IDC_BTN_BROWSE && code == BN_CLICKED) {
            OnBrowseGameDir();
        } else if (id == IDC_BTN_NEWCODE && code == BN_CLICKED) {
            char roomCode[16] = {};
            GenerateRandomRoomCode(roomCode, sizeof(roomCode));
            SetWindowTextA(g_EditRoomCode, roomCode);
            SetStatusText("Generated a new room code.");
        } else if (id == IDC_BTN_COPYCODE && code == BN_CLICKED) {
            OnCopyRoomCode();
        } else if ((id == IDC_RADIO_1V1 || id == IDC_RADIO_2V2) && code == BN_CLICKED) {
            bool is2v2 = Is2v2Selected();
            PopulateSlotCombo(is2v2, GetSelectedSlot());
            SetStatusText(is2v2
                ? "2v2 Team Duel selected! Share the Room Code with 3 friends."
                : "1v1 Duel selected! Share the Room Code with your friend.");
        } else if (id == IDC_COMBO_SLOT && code == CBN_SELCHANGE) {
            int slot = GetSelectedSlot();
            EnableWindow(g_ComboBalls, !g_Busy && slot == 0);
        } else if (id == IDC_BTN_CREATELB && code == BN_CLICKED) {
            OnCreateLeaderboard();
        } else if (id == IDC_BTN_OPENLB && code == BN_CLICKED) {
            OnOpenLeaderboardWeb();
        } else if (id == IDC_BTN_PLAY_ONLINE && code == BN_CLICKED) {
            OnPlayOnline();
        } else if (id == IDC_BTN_PLAY_OFF && code == BN_CLICKED) {
            OnPlayOffline();
        } else if (id == IDC_BTN_UNINSTALL && code == BN_CLICKED) {
            OnUninstallMod();
        }
        return 0;
    }

    case WM_APP_WAKE_DONE: {
        g_Busy = false;
        SetUiEnabled(true);
        if (wParam == 1) {
            SetStatusText("Server awake! Launching Peggle Nights...");
            LaunchPeggleGame(g_PendingLaunchDir.c_str());
        } else {
            char msg[320];
            snprintf(msg, sizeof(msg),
                "Could not reach the relay server right now (code %u).\n"
                "If your Render server is still booting up, launching anyway is fine—the mod keeps retrying automatically.\n\n"
                "Launch Peggle Nights anyway?",
                (unsigned int)lParam);
            if (MessageBoxA(hwnd, msg, "Relay Server Wake Check", MB_YESNO | MB_ICONWARNING) == IDYES) {
                SetStatusText("Launching Peggle Nights (mod will keep connecting in background)...");
                LaunchPeggleGame(g_PendingLaunchDir.c_str());
            } else {
                SetStatusText("Ready. Check your Relay Server URL or try again.");
            }
        }
        return 0;
    }

    case WM_APP_CREATELB_DONE: {
        g_Busy = false;
        SetUiEnabled(true);
        CreateLbResult* res = (CreateLbResult*)lParam;
        if (res && res->ok && !res->code.empty()) {
            SetWindowTextA(g_EditLbCode, res->code.c_str());
            WritePrivateProfileStringA("Relay", "leaderboard_code", res->code.c_str(), g_RelayConfigIni);
            std::string fullUrl = res->baseUrl + "/leaderboard/" + res->code;
            CopyTextToClipboard(fullUrl.c_str());

            char st[256];
            snprintf(st, sizeof(st), "Leaderboard %s ready! Web link copied to clipboard.", res->code.c_str());
            SetStatusText(st);

            char msg[384];
            snprintf(msg, sizeof(msg),
                "Custom leaderboard '%s' is ready!\n\n"
                "Share this code with your friends: %s\n"
                "Link (copied to clipboard): %s\n\n"
                "Open it in your browser now?",
                res->code.c_str(), res->code.c_str(), fullUrl.c_str());
            if (MessageBoxA(hwnd, msg, "Leaderboard Ready!", MB_YESNO | MB_ICONINFORMATION) == IDYES) {
                ShellExecuteA(nullptr, "open", fullUrl.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            }
        } else {
            char msg[256];
            snprintf(msg, sizeof(msg), "Could not create leaderboard right now (status %u).\nIf Render is asleep, click Play Online or retry in a few seconds.",
                     res ? (unsigned int)res->status : 0);
            MessageBoxA(hwnd, msg, "Leaderboard Error", MB_ICONWARNING);
            SetStatusText("Could not reach server to create leaderboard.");
        }
        delete res;
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE /*hPrev*/, LPSTR /*lpCmd*/, int nCmdShow) {
    SetProcessDPIAware();
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    InitCommonControls();

    GetModuleFileNameA(nullptr, g_LauncherDir, MAX_PATH);
    char* slash = strrchr(g_LauncherDir, '\\');
    if (slash) *(slash + 1) = '\0';
    snprintf(g_RelayConfigIni, sizeof(g_RelayConfigIni), "%srelay_config.ini", g_LauncherDir);

    HDC hdc = GetDC(nullptr);
    g_Dpi = GetDeviceCaps(hdc, LOGPIXELSX);
    ReleaseDC(nullptr, hdc);

    WNDCLASSEXA wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = MainWndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorA(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconA(nullptr, IDI_APPLICATION);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "PeggleOnlineLauncherWnd";
    RegisterClassExA(&wc);

    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT rc = { 0, 0, S(500), S(510) };
    AdjustWindowRectEx(&rc, style, FALSE, 0);

    HWND hwnd = CreateWindowExA(
        0,
        "PeggleOnlineLauncherWnd",
        "Peggle Nights Online Duel Launcher",
        style,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top,
        nullptr, nullptr, hInst, nullptr
    );
    if (!hwnd) return 1;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageA(&msg, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageA(hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }
    CoUninitialize();
    return (int)msg.wParam;
}
