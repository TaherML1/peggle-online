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
#define IDC_RADIO_HOST      104
#define IDC_RADIO_JOIN      105
#define IDC_EDIT_ROOMCODE   106
#define IDC_BTN_NEWCODE     107
#define IDC_BTN_COPYCODE    108
#define IDC_BTN_PLAY_ONLINE 109
#define IDC_BTN_PLAY_OFF    110
#define IDC_BTN_UNINSTALL   111
#define IDC_STATIC_STATUS   112

#define WM_APP_WAKE_DONE    (WM_USER + 1)

static const char* kDefaultRelayUrl = "https://peggle-online-relay.onrender.com";

static int   g_Dpi = 96;
static HWND  g_HwndMain = nullptr;
static HWND  g_EditGameDir = nullptr;
static HWND  g_BtnBrowse = nullptr;
static HWND  g_EditRelayUrl = nullptr;
static HWND  g_RadioHost = nullptr;
static HWND  g_RadioJoin = nullptr;
static HWND  g_EditRoomCode = nullptr;
static HWND  g_BtnNewCode = nullptr;
static HWND  g_BtnCopyCode = nullptr;
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
static bool  g_Waking = false;

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
    // 1. Check saved game_dir from relay_config.ini
    char savedDir[MAX_PATH] = {};
    GetPrivateProfileStringA("Relay", "game_dir", "", savedDir, sizeof(savedDir), g_RelayConfigIni);
    TrimTrailingSlash(savedDir);
    if (IsValidGameDir(savedDir)) {
        snprintf(outDir, outCap, "%s", savedDir);
        return true;
    }

    // 2. Check launcher directory itself
    char selfDir[MAX_PATH];
    snprintf(selfDir, sizeof(selfDir), "%s", g_LauncherDir);
    TrimTrailingSlash(selfDir);
    if (IsValidGameDir(selfDir)) {
        snprintf(outDir, outCap, "%s", selfDir);
        return true;
    }

    // 3. Check Steam registry + libraryfolders.vdf
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

    // 4. Common non-Steam PopCap fallbacks
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

static void SetStatusText(const char* text) {
    SetWindowTextA(g_StaticStatus, text);
}

static void SetUiEnabled(bool enabled) {
    EnableWindow(g_EditGameDir, enabled);
    EnableWindow(g_BtnBrowse, enabled);
    EnableWindow(g_EditRelayUrl, enabled);
    EnableWindow(g_RadioHost, enabled);
    EnableWindow(g_RadioJoin, enabled);
    EnableWindow(g_EditRoomCode, enabled);
    EnableWindow(g_BtnNewCode, enabled);
    EnableWindow(g_BtnCopyCode, enabled);
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
        if (isalnum(c)) {
            out[w++] = (char)toupper(c);
        }
    }
    out[w] = '\0';
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

static bool WriteGameIni(const char* gameDir, bool enabled, const char* role, const char* relayUrl, const char* roomCode) {
    char iniPath[MAX_PATH];
    snprintf(iniPath, sizeof(iniPath), "%s\\peggle_online.ini", gameDir);
    FILE* f = fopen(iniPath, "w");
    if (!f) return false;
    fprintf(f, "[Online]\n");
    fprintf(f, "enabled = %d\n", enabled ? 1 : 0);
    fprintf(f, "role = %s\n", role);
    fprintf(f, "relay_url = %s\n", relayUrl);
    fprintf(f, "room_code = %s\n", roomCode);
    fprintf(f, "host_ip = 127.0.0.1\n");
    fprintf(f, "port = 27888\n");
    fclose(f);
    return true;
}

static void SaveRelayConfigIni(const char* relayUrl, const char* gameDir) {
    WritePrivateProfileStringA("Relay", "relay_url", relayUrl, g_RelayConfigIni);
    WritePrivateProfileStringA("Relay", "game_dir", gameDir, g_RelayConfigIni);
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

struct WakeThreadArgs {
    std::string relayUrl;
    std::string gameDir;
};

static bool WakeRelayHealth(const char* rawUrl, DWORD* outStatusOrErr) {
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

    wchar_t wideUrl[320] = {};
    MultiByteToWideChar(CP_UTF8, 0, normalized, -1, wideUrl, 320);

    wchar_t hostName[160] = {};
    wchar_t basePath[160] = {};
    URL_COMPONENTSW uc = {};
    uc.dwStructSize = sizeof(uc);
    uc.lpszHostName = hostName;
    uc.dwHostNameLength = 160;
    uc.lpszUrlPath = basePath;
    uc.dwUrlPathLength = 160;

    if (!WinHttpCrackUrl(wideUrl, 0, 0, &uc) || hostName[0] == L'\0') {
        *outStatusOrErr = ERROR_INVALID_PARAMETER;
        return false;
    }

    size_t plen = wcslen(basePath);
    while (plen > 0 && basePath[plen - 1] == L'/') {
        basePath[--plen] = L'\0';
    }
    wchar_t healthPath[200] = {};
    _snwprintf(healthPath, 200, L"%s/health", basePath);
    healthPath[199] = L'\0';

    bool isSecure = (uc.nScheme == INTERNET_SCHEME_HTTPS);

    HINTERNET hSession = WinHttpOpen(
        L"PeggleOnlineLauncher/2.0",
        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
    );
    if (!hSession) {
        *outStatusOrErr = GetLastError();
        return false;
    }

    WinHttpSetTimeouts(hSession, 10000, 15000, 15000, 45000);

    HINTERNET hConnect = WinHttpConnect(hSession, hostName, uc.nPort, 0);
    if (!hConnect) {
        *outStatusOrErr = GetLastError();
        WinHttpCloseHandle(hSession);
        return false;
    }

    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect,
        L"GET",
        healthPath,
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        isSecure ? WINHTTP_FLAG_SECURE : 0
    );
    if (!hRequest) {
        *outStatusOrErr = GetLastError();
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    bool ok = false;
    if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(hRequest, nullptr)) {
        DWORD status = 0;
        DWORD statusSz = sizeof(status);
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
    } else {
        *outStatusOrErr = GetLastError();
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return ok;
}

static DWORD WINAPI WakeServerThread(LPVOID param) {
    WakeThreadArgs* args = (WakeThreadArgs*)param;
    DWORD code = 0;
    bool ok = WakeRelayHealth(args->relayUrl.c_str(), &code);
    delete args;
    PostMessageA(g_HwndMain, WM_APP_WAKE_DONE, ok ? 1 : 0, (LPARAM)code);
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
    char raw[32] = {};
    char clean[32] = {};
    GetWindowTextA(g_EditRoomCode, raw, sizeof(raw));
    SanitizeRoomCode(raw, clean, sizeof(clean));
    if (!clean[0]) {
        SetStatusText("Enter a room code first.");
        return;
    }
    SetWindowTextA(g_EditRoomCode, clean);

    if (OpenClipboard(g_HwndMain)) {
        EmptyClipboard();
        size_t len = strlen(clean) + 1;
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
        if (hMem) {
            void* ptr = GlobalLock(hMem);
            memcpy(ptr, clean, len);
            GlobalUnlock(hMem);
            SetClipboardData(CF_TEXT, hMem);
        }
        CloseClipboard();
        char msg[128];
        snprintf(msg, sizeof(msg), "Room code %s copied! Send it to your friend.", clean);
        SetStatusText(msg);
    }
}

static std::string g_PendingLaunchDir;

static void OnPlayOnline() {
    if (g_Waking) return;

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
        MessageBoxA(
            g_HwndMain,
            "Please enter your Render relay server URL.",
            "Relay URL Missing",
            MB_ICONWARNING
        );
        return;
    }

    char rawCode[32] = {};
    char roomCode[32] = {};
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

    bool isHost = (SendMessageA(g_RadioHost, BM_GETCHECK, 0, 0) == BST_CHECKED);
    const char* roleStr = isHost ? "ws_host" : "ws_client";

    char errBuf[512] = {};
    if (!InstallModToGameDir(gameDir, errBuf, sizeof(errBuf))) {
        MessageBoxA(g_HwndMain, errBuf, "Mod Install Error", MB_ICONERROR);
        return;
    }

    SaveRelayConfigIni(relayUrl, gameDir);
    if (!WriteGameIni(gameDir, true, roleStr, relayUrl, roomCode)) {
        MessageBoxA(
            g_HwndMain,
            "Failed to write peggle_online.ini in the game folder.\nTry running as Administrator if the game is in Program Files.",
            "Config Write Error",
            MB_ICONERROR
        );
        return;
    }

    g_Waking = true;
    g_PendingLaunchDir = gameDir;
    SetUiEnabled(false);
    SetStatusText("Waking up Render server (first load can take ~20s)...");

    WakeThreadArgs* args = new WakeThreadArgs();
    args->relayUrl = relayUrl;
    args->gameDir = gameDir;
    HANDLE hThread = CreateThread(nullptr, 0, WakeServerThread, args, 0, nullptr);
    if (hThread) {
        CloseHandle(hThread);
    } else {
        delete args;
        g_Waking = false;
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
        WriteGameIni(gameDir, false, "ws_host", relayUrl[0] ? relayUrl : kDefaultRelayUrl, "PEGG");
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

    char dstDll[MAX_PATH];
    char origDll[MAX_PATH];
    char iniPath[MAX_PATH];
    char logPath[MAX_PATH];
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

    makeLabel("Peggle Nights Online Duel", 18, 14, 440, 26, g_FontTitle);
    makeLabel("Auto-installs the mod and connects you & your friend with a Room Code.", 18, 42, 444, 20, g_FontNormal);

    makeLabel("Peggle Nights Folder:", 18, 72, 200, 18, g_FontBold);
    g_EditGameDir = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_READONLY,
        S(18), S(93), S(356), S(24), hwnd, (HMENU)IDC_EDIT_GAMEDIR, nullptr, nullptr);
    SendMessageA(g_EditGameDir, WM_SETFONT, (WPARAM)g_FontNormal, TRUE);

    g_BtnBrowse = CreateWindowExA(0, "BUTTON", "Browse...",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        S(382), S(92), S(82), S(26), hwnd, (HMENU)IDC_BTN_BROWSE, nullptr, nullptr);
    SendMessageA(g_BtnBrowse, WM_SETFONT, (WPARAM)g_FontNormal, TRUE);

    makeLabel("Relay Server URL (Render):", 18, 128, 250, 18, g_FontBold);
    g_EditRelayUrl = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
        S(18), S(149), S(446), S(24), hwnd, (HMENU)IDC_EDIT_RELAYURL, nullptr, nullptr);
    SendMessageA(g_EditRelayUrl, WM_SETFONT, (WPARAM)g_FontNormal, TRUE);

    makeLabel("Your Role:", 18, 184, 150, 18, g_FontBold);
    g_RadioHost = CreateWindowExA(0, "BUTTON", "Host a Match (Player 1)",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTORADIOBUTTON | WS_GROUP,
        S(18), S(205), S(210), S(22), hwnd, (HMENU)IDC_RADIO_HOST, nullptr, nullptr);
    SendMessageA(g_RadioHost, WM_SETFONT, (WPARAM)g_FontNormal, TRUE);

    g_RadioJoin = CreateWindowExA(0, "BUTTON", "Join a Match (Player 2)",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTORADIOBUTTON,
        S(240), S(205), S(210), S(22), hwnd, (HMENU)IDC_RADIO_JOIN, nullptr, nullptr);
    SendMessageA(g_RadioJoin, WM_SETFONT, (WPARAM)g_FontNormal, TRUE);
    SendMessageA(g_RadioHost, BM_SETCHECK, BST_CHECKED, 0);

    makeLabel("Room Code (share this 4-letter code with your friend):", 18, 238, 400, 18, g_FontBold);
    g_EditRoomCode = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_UPPERCASE | ES_CENTER,
        S(18), S(260), S(150), S(30), hwnd, (HMENU)IDC_EDIT_ROOMCODE, nullptr, nullptr);
    SendMessageA(g_EditRoomCode, EM_LIMITTEXT, 12, 0);
    SendMessageA(g_EditRoomCode, WM_SETFONT, (WPARAM)g_FontCode, TRUE);

    g_BtnNewCode = CreateWindowExA(0, "BUTTON", "New Code",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        S(178), S(260), S(96), S(30), hwnd, (HMENU)IDC_BTN_NEWCODE, nullptr, nullptr);
    SendMessageA(g_BtnNewCode, WM_SETFONT, (WPARAM)g_FontNormal, TRUE);

    g_BtnCopyCode = CreateWindowExA(0, "BUTTON", "Copy Code",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        S(282), S(260), S(96), S(30), hwnd, (HMENU)IDC_BTN_COPYCODE, nullptr, nullptr);
    SendMessageA(g_BtnCopyCode, WM_SETFONT, (WPARAM)g_FontNormal, TRUE);

    g_StaticStatus = makeLabel("Ready! Click 'Play Online Duel!' to install & launch.", 18, 304, 446, 36, g_FontNormal, IDC_STATIC_STATUS);

    g_BtnPlayOnline = CreateWindowExA(0, "BUTTON", "Play Online Duel!",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
        S(18), S(348), S(190), S(36), hwnd, (HMENU)IDC_BTN_PLAY_ONLINE, nullptr, nullptr);
    SendMessageA(g_BtnPlayOnline, WM_SETFONT, (WPARAM)g_FontBold, TRUE);

    g_BtnPlayOffline = CreateWindowExA(0, "BUTTON", "Play Offline",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        S(218), S(348), S(118), S(36), hwnd, (HMENU)IDC_BTN_PLAY_OFF, nullptr, nullptr);
    SendMessageA(g_BtnPlayOffline, WM_SETFONT, (WPARAM)g_FontNormal, TRUE);

    g_BtnUninstall = CreateWindowExA(0, "BUTTON", "Uninstall Mod",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        S(346), S(348), S(118), S(36), hwnd, (HMENU)IDC_BTN_UNINSTALL, nullptr, nullptr);
    SendMessageA(g_BtnUninstall, WM_SETFONT, (WPARAM)g_FontNormal, TRUE);

    // Populate initial values
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
        } else if (id == IDC_RADIO_HOST && code == BN_CLICKED) {
            char cur[32] = {};
            GetWindowTextA(g_EditRoomCode, cur, sizeof(cur));
            TrimWhitespace(cur);
            if (!cur[0]) {
                char roomCode[16] = {};
                GenerateRandomRoomCode(roomCode, sizeof(roomCode));
                SetWindowTextA(g_EditRoomCode, roomCode);
            }
            SetStatusText("Hosting as Player 1. Share the Room Code with your friend!");
        } else if (id == IDC_RADIO_JOIN && code == BN_CLICKED) {
            SetStatusText("Joining as Player 2. Paste your friend's Room Code above.");
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
        g_Waking = false;
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
            int choice = MessageBoxA(hwnd, msg, "Relay Server Wake Check", MB_YESNO | MB_ICONWARNING);
            if (choice == IDYES) {
                SetStatusText("Launching Peggle Nights (mod will keep connecting in background)...");
                LaunchPeggleGame(g_PendingLaunchDir.c_str());
            } else {
                SetStatusText("Ready. Check your Relay Server URL or try again.");
            }
        }
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
    RECT rc = { 0, 0, S(482), S(404) };
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
