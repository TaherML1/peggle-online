#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <winhttp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <vector>
#include <mutex>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "winhttp.lib")

static const char* kModMarker = "PEGGLE_ONLINE_MOD_V2";

// ============================================================================
// 1. BASS.DLL Proxy Forwarding Table (95 exports forwarded to bass_orig.dll)
// ============================================================================
extern "C" FARPROC g_BassProcs[95] = { 0 };

static const char* kBassExportNames[95] = {
    "BASS_Apply3D",
    "BASS_ChannelBytes2Seconds",
    "BASS_ChannelGet3DAttributes",
    "BASS_ChannelGet3DPosition",
    "BASS_ChannelGetAttributes",
    "BASS_ChannelGetData",
    "BASS_ChannelGetDevice",
    "BASS_ChannelGetEAXMix",
    "BASS_ChannelGetInfo",
    "BASS_ChannelGetLength",
    "BASS_ChannelGetLevel",
    "BASS_ChannelGetPosition",
    "BASS_ChannelIsActive",
    "BASS_ChannelIsSliding",
    "BASS_ChannelPause",
    "BASS_ChannelPlay",
    "BASS_ChannelPreBuf",
    "BASS_ChannelRemoveDSP",
    "BASS_ChannelRemoveFX",
    "BASS_ChannelRemoveLink",
    "BASS_ChannelRemoveSync",
    "BASS_ChannelSeconds2Bytes",
    "BASS_ChannelSet3DAttributes",
    "BASS_ChannelSet3DPosition",
    "BASS_ChannelSetAttributes",
    "BASS_ChannelSetDSP",
    "BASS_ChannelSetEAXMix",
    "BASS_ChannelSetFX",
    "BASS_ChannelSetFlags",
    "BASS_ChannelSetLink",
    "BASS_ChannelSetPosition",
    "BASS_ChannelSetSync",
    "BASS_ChannelSlideAttributes",
    "BASS_ChannelStop",
    "BASS_ErrorGetCode",
    "BASS_FXGetParameters",
    "BASS_FXSetParameters",
    "BASS_Free",
    "BASS_Get3DFactors",
    "BASS_Get3DPosition",
    "BASS_GetCPU",
    "BASS_GetConfig",
    "BASS_GetDSoundObject",
    "BASS_GetDevice",
    "BASS_GetDeviceDescription",
    "BASS_GetEAXParameters",
    "BASS_GetInfo",
    "BASS_GetVersion",
    "BASS_GetVolume",
    "BASS_Init",
    "BASS_MusicFree",
    "BASS_MusicGetAttribute",
    "BASS_MusicGetName",
    "BASS_MusicGetOrderPosition",
    "BASS_MusicGetOrders",
    "BASS_MusicLoad",
    "BASS_MusicSetAttribute",
    "BASS_Pause",
    "BASS_PluginFree",
    "BASS_PluginLoad",
    "BASS_RecordFree",
    "BASS_RecordGetDevice",
    "BASS_RecordGetDeviceDescription",
    "BASS_RecordGetInfo",
    "BASS_RecordGetInput",
    "BASS_RecordGetInputName",
    "BASS_RecordInit",
    "BASS_RecordSetDevice",
    "BASS_RecordSetInput",
    "BASS_RecordStart",
    "BASS_SampleCreate",
    "BASS_SampleCreateDone",
    "BASS_SampleFree",
    "BASS_SampleGetChannel",
    "BASS_SampleGetInfo",
    "BASS_SampleLoad",
    "BASS_SampleSetInfo",
    "BASS_SampleStop",
    "BASS_Set3DFactors",
    "BASS_Set3DPosition",
    "BASS_SetConfig",
    "BASS_SetDevice",
    "BASS_SetEAXParameters",
    "BASS_SetVolume",
    "BASS_Start",
    "BASS_Stop",
    "BASS_StreamCreate",
    "BASS_StreamCreateFile",
    "BASS_StreamCreateFileUser",
    "BASS_StreamCreateURL",
    "BASS_StreamFree",
    "BASS_StreamGetFilePosition",
    "BASS_StreamGetTags",
    "BASS_Update",
    "_"
};

#define DEFINE_BASS_THUNK(idx, name) \
    extern "C" __declspec(naked) void __cdecl proxy_##name() { \
        __asm { jmp dword ptr [g_BassProcs + idx * 4] } \
    }

DEFINE_BASS_THUNK(0, BASS_Apply3D)
DEFINE_BASS_THUNK(1, BASS_ChannelBytes2Seconds)
DEFINE_BASS_THUNK(2, BASS_ChannelGet3DAttributes)
DEFINE_BASS_THUNK(3, BASS_ChannelGet3DPosition)
DEFINE_BASS_THUNK(4, BASS_ChannelGetAttributes)
DEFINE_BASS_THUNK(5, BASS_ChannelGetData)
DEFINE_BASS_THUNK(6, BASS_ChannelGetDevice)
DEFINE_BASS_THUNK(7, BASS_ChannelGetEAXMix)
DEFINE_BASS_THUNK(8, BASS_ChannelGetInfo)
DEFINE_BASS_THUNK(9, BASS_ChannelGetLength)
DEFINE_BASS_THUNK(10, BASS_ChannelGetLevel)
DEFINE_BASS_THUNK(11, BASS_ChannelGetPosition)
DEFINE_BASS_THUNK(12, BASS_ChannelIsActive)
DEFINE_BASS_THUNK(13, BASS_ChannelIsSliding)
DEFINE_BASS_THUNK(14, BASS_ChannelPause)
DEFINE_BASS_THUNK(15, BASS_ChannelPlay)
DEFINE_BASS_THUNK(16, BASS_ChannelPreBuf)
DEFINE_BASS_THUNK(17, BASS_ChannelRemoveDSP)
DEFINE_BASS_THUNK(18, BASS_ChannelRemoveFX)
DEFINE_BASS_THUNK(19, BASS_ChannelRemoveLink)
DEFINE_BASS_THUNK(20, BASS_ChannelRemoveSync)
DEFINE_BASS_THUNK(21, BASS_ChannelSeconds2Bytes)
DEFINE_BASS_THUNK(22, BASS_ChannelSet3DAttributes)
DEFINE_BASS_THUNK(23, BASS_ChannelSet3DPosition)
DEFINE_BASS_THUNK(24, BASS_ChannelSetAttributes)
DEFINE_BASS_THUNK(25, BASS_ChannelSetDSP)
DEFINE_BASS_THUNK(26, BASS_ChannelSetEAXMix)
DEFINE_BASS_THUNK(27, BASS_ChannelSetFX)
DEFINE_BASS_THUNK(28, BASS_ChannelSetFlags)
DEFINE_BASS_THUNK(29, BASS_ChannelSetLink)
DEFINE_BASS_THUNK(30, BASS_ChannelSetPosition)
DEFINE_BASS_THUNK(31, BASS_ChannelSetSync)
DEFINE_BASS_THUNK(32, BASS_ChannelSlideAttributes)
DEFINE_BASS_THUNK(33, BASS_ChannelStop)
DEFINE_BASS_THUNK(34, BASS_ErrorGetCode)
DEFINE_BASS_THUNK(35, BASS_FXGetParameters)
DEFINE_BASS_THUNK(36, BASS_FXSetParameters)
DEFINE_BASS_THUNK(37, BASS_Free)
DEFINE_BASS_THUNK(38, BASS_Get3DFactors)
DEFINE_BASS_THUNK(39, BASS_Get3DPosition)
DEFINE_BASS_THUNK(40, BASS_GetCPU)
DEFINE_BASS_THUNK(41, BASS_GetConfig)
DEFINE_BASS_THUNK(42, BASS_GetDSoundObject)
DEFINE_BASS_THUNK(43, BASS_GetDevice)
DEFINE_BASS_THUNK(44, BASS_GetDeviceDescription)
DEFINE_BASS_THUNK(45, BASS_GetEAXParameters)
DEFINE_BASS_THUNK(46, BASS_GetInfo)
DEFINE_BASS_THUNK(47, BASS_GetVersion)
DEFINE_BASS_THUNK(48, BASS_GetVolume)
DEFINE_BASS_THUNK(49, BASS_Init)
DEFINE_BASS_THUNK(50, BASS_MusicFree)
DEFINE_BASS_THUNK(51, BASS_MusicGetAttribute)
DEFINE_BASS_THUNK(52, BASS_MusicGetName)
DEFINE_BASS_THUNK(53, BASS_MusicGetOrderPosition)
DEFINE_BASS_THUNK(54, BASS_MusicGetOrders)
DEFINE_BASS_THUNK(55, BASS_MusicLoad)
DEFINE_BASS_THUNK(56, BASS_MusicSetAttribute)
DEFINE_BASS_THUNK(57, BASS_Pause)
DEFINE_BASS_THUNK(58, BASS_PluginFree)
DEFINE_BASS_THUNK(59, BASS_PluginLoad)
DEFINE_BASS_THUNK(60, BASS_RecordFree)
DEFINE_BASS_THUNK(61, BASS_RecordGetDevice)
DEFINE_BASS_THUNK(62, BASS_RecordGetDeviceDescription)
DEFINE_BASS_THUNK(63, BASS_RecordGetInfo)
DEFINE_BASS_THUNK(64, BASS_RecordGetInput)
DEFINE_BASS_THUNK(65, BASS_RecordGetInputName)
DEFINE_BASS_THUNK(66, BASS_RecordInit)
DEFINE_BASS_THUNK(67, BASS_RecordSetDevice)
DEFINE_BASS_THUNK(68, BASS_RecordSetInput)
DEFINE_BASS_THUNK(69, BASS_RecordStart)
DEFINE_BASS_THUNK(70, BASS_SampleCreate)
DEFINE_BASS_THUNK(71, BASS_SampleCreateDone)
DEFINE_BASS_THUNK(72, BASS_SampleFree)
DEFINE_BASS_THUNK(73, BASS_SampleGetChannel)
DEFINE_BASS_THUNK(74, BASS_SampleGetInfo)
DEFINE_BASS_THUNK(75, BASS_SampleLoad)
DEFINE_BASS_THUNK(76, BASS_SampleSetInfo)
DEFINE_BASS_THUNK(77, BASS_SampleStop)
DEFINE_BASS_THUNK(78, BASS_Set3DFactors)
DEFINE_BASS_THUNK(79, BASS_Set3DPosition)
DEFINE_BASS_THUNK(80, BASS_SetConfig)
DEFINE_BASS_THUNK(81, BASS_SetDevice)
DEFINE_BASS_THUNK(82, BASS_SetEAXParameters)
DEFINE_BASS_THUNK(83, BASS_SetVolume)
DEFINE_BASS_THUNK(84, BASS_Start)
DEFINE_BASS_THUNK(85, BASS_Stop)
DEFINE_BASS_THUNK(86, BASS_StreamCreate)
DEFINE_BASS_THUNK(87, BASS_StreamCreateFile)
DEFINE_BASS_THUNK(88, BASS_StreamCreateFileUser)
DEFINE_BASS_THUNK(89, BASS_StreamCreateURL)
DEFINE_BASS_THUNK(90, BASS_StreamFree)
DEFINE_BASS_THUNK(91, BASS_StreamGetFilePosition)
DEFINE_BASS_THUNK(92, BASS_StreamGetTags)
DEFINE_BASS_THUNK(93, BASS_Update)
DEFINE_BASS_THUNK(94, _)

// ============================================================================
// 2. Reverse-Engineered Peggle Nights (Steam) Addresses & Engine Calls
// ============================================================================
static constexpr uintptr_t ADDR_G_SEXY_APP_BASE       = 0x006cbe04;
static constexpr uintptr_t ADDR_APP_VTABLE            = 0x0068670c;
static constexpr uintptr_t ADDR_BOARD_VTABLE          = 0x006864a4;
static constexpr uintptr_t ADDR_MT_STATE              = 0x00704268; // 624 uint32 + 1 uint32 index = 2500 bytes
static constexpr size_t    MT_STATE_BYTES             = 2500;

static constexpr uintptr_t ADDR_FN_BOARD_SAVESTATE    = 0x00405d00;
static constexpr uintptr_t ADDR_FN_BOARD_LOADSTATE    = 0x00405db0;
static constexpr uintptr_t ADDR_FN_IFACE_SETNUMBALLS  = 0x00463c10;
static constexpr uintptr_t ADDR_FN_CALC_MOUSE_AIM     = 0x00468940;
static constexpr uintptr_t ADDR_FN_GUN_SETANGLE       = 0x00498c50;

struct FakeBuffer {
    uint32_t pad0;
    uint32_t pad1;
    const void* dataPtr; // +0x8
    uint32_t dataLen;    // +0xc
};

static bool Engine_Board_SaveState(void* board, std::vector<uint8_t>& outBytes) {
    if (!board) return false;
    typedef void* (__stdcall *FnSaveState)(void* b);
    FnSaveState fn = (FnSaveState)ADDR_FN_BOARD_SAVESTATE;
    void* writer = fn(board);
    if (!writer) return false;

    const uint8_t* ptr = *(const uint8_t**)((char*)writer + 0x8);
    uint32_t len = *(uint32_t*)((char*)writer + 0xc);
    bool ok = false;
    if (ptr && len > 0 && len < 16 * 1024 * 1024) {
        outBytes.assign(ptr, ptr + len);
        ok = true;
    }

    // Free Sexy::DataWriter via virtual destructor slot 0
    typedef void (__thiscall *FnScalarDtor)(void* self, int flags);
    void** vtbl = *(void***)writer;
    if (vtbl && vtbl[0]) {
        ((FnScalarDtor)vtbl[0])(writer, 1);
    }
    return ok;
}

static bool Engine_Board_LoadState(void* board, const uint8_t* dataPtr, uint32_t dataLen) {
    if (!board || !dataPtr || dataLen == 0) return false;
    FakeBuffer fb = { 0, 0, dataPtr, dataLen };
    FakeBuffer* pFb = &fb;
    uint32_t fn = ADDR_FN_BOARD_LOADSTATE;
    uint8_t res = 0;
    __asm {
        push 0
        mov ecx, pFb
        mov edx, board
        call fn
        mov res, al
    }
    return res != 0;
}

static void Engine_Gun_SetAngle(void* gun, float angle, int immediate) {
    if (!gun) return;
    uint32_t fn = ADDR_FN_GUN_SETANGLE;
    __asm {
        push immediate
        push dword ptr [angle]
        mov eax, gun
        call fn
    }
}

static void Engine_Logic_CalcMouseAim(void* logic) {
    if (!logic) return;
    uint32_t fn = ADDR_FN_CALC_MOUSE_AIM;
    __asm {
        push 1
        mov eax, logic
        call fn
    }
}

static void Engine_Interface_SetNumBalls(void* iface, int numBalls) {
    if (!iface) return;
    uint32_t fn = ADDR_FN_IFACE_SETNUMBALLS;
    __asm {
        push ebx
        mov eax, numBalls
        mov ebx, iface
        call fn
        pop ebx
    }
}

// ============================================================================
// 3. Mod Configuration & Network Protocol
// ============================================================================
enum NetRole {
    ROLE_HOST         = 0, // Direct TCP Listen (Player 1)
    ROLE_CLIENT       = 1, // Direct TCP Connect (Player 2)
    ROLE_RELAY_HOST   = 2, // Outbound TCP to Relay Server (Player 1)
    ROLE_RELAY_CLIENT = 3, // Outbound TCP to Relay Server (Player 2)
    ROLE_WS_HOST      = 4, // WebSocket Room Relay (Slot 0)
    ROLE_WS_CLIENT    = 5  // WebSocket Room Relay (Slots 1..3)
};

static bool     g_ModEnabled          = true;
static NetRole  g_Role                = ROLE_HOST;
static int      g_ModePlayers         = 2;     // 2 = 1v1, 4 = 2v2
static int      g_LocalPlayerSlot     = 0;     // 0..3 (team = slot % 2)
static int      g_LocalPlayerIdx      = 0;     // Mirror of g_LocalPlayerSlot
static int      g_BallsPerPlayer      = 0;     // 0 = vanilla default, 3..6 = custom
static char     g_PlayerName[48]      = "Player";
static char     g_LeaderboardCode[32] = "";
static char     g_HostIp[128]         = "127.0.0.1";
static int      g_Port                = 27888;
static char     g_RelayUrl[256]       = "";
static char     g_RoomCode[32]        = "PEGG";
static char     g_ModDir[MAX_PATH]    = { 0 };
static FILE*    g_LogFile             = nullptr;

static void LogMsg(const char* fmt, ...) {
    if (!g_LogFile) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(g_LogFile, fmt, ap);
    fprintf(g_LogFile, "\n");
    fflush(g_LogFile);
    va_end(ap);
}

enum PacketType : uint8_t {
    PKT_STATE_SYNC   = 1, // [uint8_t isInitial][2500B MT][uint32_t len][len B BoardState]
    PKT_AIM          = 2, // [float angle]
    PKT_FIRE         = 3, // [float angle][2500B MT]
    PKT_FLIGHT_CLICK = 4, // [uint8_t isDown][int32_t clickCount][int32_t x][int32_t y]
    PKT_MATCH_RESULT = 5  // [UTF-8 JSON leaderboard match payload]
};

struct QueuedStateSync {
    bool valid = false;
    bool isInitial = false;
    uint32_t receivedTick = 0;
    uint8_t mtState[MT_STATE_BYTES];
    std::vector<uint8_t> boardBytes;
};

struct QueuedFire {
    bool valid = false;
    float angle = 0.0f;
    uint8_t mtState[MT_STATE_BYTES];
};

struct QueuedClick {
    uint8_t isDown;
    int32_t clickCount;
    int32_t x;
    int32_t y;
};

static std::mutex               g_NetMutex;
static std::mutex               g_SendMutex;
static std::mutex               g_HudMutex;
static SOCKET                   g_PeerSock           = INVALID_SOCKET;
static HINTERNET                g_WsHandle           = nullptr;
static volatile bool            g_RelayOnline        = false;
static volatile bool            g_Connected          = false;
static volatile bool            g_WantReconnect      = false;
static volatile int             g_RoomConnectedCount = 0;
static volatile int             g_RoomMaxPlayers     = 2;
static bool                     g_PeerJustJoined     = false;
static bool                     g_HasRemoteAim       = false;
static float                    g_RemoteAimAngle     = 0.0f;
static QueuedStateSync          g_PendingSync;
static QueuedFire               g_PendingFire;
static std::vector<QueuedClick> g_PendingClicks;

static void UpdateWindowTitleHUD(void* board);
static void PollHotkeys(void* board);

static void TickMenuTitleAndHotkeys() {
    __try {
        void* app = *(void**)ADDR_G_SEXY_APP_BASE;
        if (!app) return;
        void* board = *(void**)((char*)app + 0x864);
        if (!board) {
            PollHotkeys(nullptr);
            UpdateWindowTitleHUD(nullptr);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        // Ignore transient startup reads
    }
}

static void CloseActiveTransport() {
    HINTERNET wsToClose = nullptr;
    SOCKET sockToClose = INVALID_SOCKET;
    {
        std::lock_guard<std::mutex> lock(g_SendMutex);
        if (g_WsHandle) {
            wsToClose = g_WsHandle;
            g_WsHandle = nullptr;
        }
        if (g_PeerSock != INVALID_SOCKET) {
            sockToClose = g_PeerSock;
            g_PeerSock = INVALID_SOCKET;
        }
    }
    g_Connected = false;
    g_RelayOnline = false;
    g_RoomConnectedCount = 0;
    if (wsToClose) {
        WinHttpCloseHandle(wsToClose);
    }
    if (sockToClose != INVALID_SOCKET) {
        closesocket(sockToClose);
    }
}

static bool SendAll(SOCKET s, const void* buf, int len) {
    const char* p = (const char*)buf;
    int total = 0;
    while (total < len) {
        int n = send(s, p + total, len - total, 0);
        if (n <= 0) return false;
        total += n;
    }
    return true;
}

static bool RecvAll(SOCKET s, void* buf, int len) {
    char* p = (char*)buf;
    int total = 0;
    while (total < len) {
        int n = recv(s, p + total, len - total, 0);
        if (n <= 0) return false;
        total += n;
    }
    return true;
}

static void SendPacket(uint8_t type, const void* payload, uint32_t len) {
    std::lock_guard<std::mutex> lock(g_SendMutex);
    if (!g_Connected && !(type == PKT_MATCH_RESULT && g_WsHandle != nullptr)) return;

    if (g_WsHandle != nullptr) {
        std::vector<uint8_t> msg(5 + len);
        msg[0] = type;
        memcpy(msg.data() + 1, &len, 4);
        if (len > 0 && payload) {
            memcpy(msg.data() + 5, payload, len);
        }
        DWORD err = WinHttpWebSocketSend(
            g_WsHandle,
            WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE,
            msg.data(),
            (DWORD)msg.size()
        );
        if (err != NO_ERROR) {
            HINTERNET h = g_WsHandle;
            g_WsHandle = nullptr;
            g_Connected = false;
            g_RelayOnline = false;
            g_RoomConnectedCount = 0;
            WinHttpCloseHandle(h);
        }
        return;
    }

    if (g_PeerSock == INVALID_SOCKET) return;
    uint8_t hdr[5];
    hdr[0] = type;
    memcpy(hdr + 1, &len, 4);
    if (!SendAll(g_PeerSock, hdr, 5) || (len > 0 && !SendAll(g_PeerSock, payload, (int)len))) {
        closesocket(g_PeerSock);
        g_PeerSock = INVALID_SOCKET;
        g_Connected = false;
    }
}

static void HandleIncomingPacket(uint8_t type, const uint8_t* data, uint32_t len) {
    std::lock_guard<std::mutex> lock(g_NetMutex);
    if (type == PKT_STATE_SYNC && len > 1 + MT_STATE_BYTES + 4) {
        uint8_t isInit = data[0];
        uint32_t stateLen = 0;
        memcpy(&stateLen, data + 1 + MT_STATE_BYTES, 4);
        if (1 + MT_STATE_BYTES + 4 + stateLen == len) {
            g_PendingSync.valid = true;
            g_PendingSync.isInitial = (isInit != 0);
            g_PendingSync.receivedTick = GetTickCount();
            memcpy(g_PendingSync.mtState, data + 1, MT_STATE_BYTES);
            g_PendingSync.boardBytes.assign(
                data + 1 + MT_STATE_BYTES + 4,
                data + len
            );
            LogMsg("Received PKT_STATE_SYNC (initial=%d, bytes=%u)", (int)isInit, stateLen);
        }
    } else if (type == PKT_AIM && len == 4) {
        memcpy(&g_RemoteAimAngle, data, 4);
        g_HasRemoteAim = true;
    } else if (type == PKT_FIRE && len == 4 + MT_STATE_BYTES) {
        memcpy(&g_PendingFire.angle, data, 4);
        memcpy(g_PendingFire.mtState, data + 4, MT_STATE_BYTES);
        g_PendingFire.valid = true;
        LogMsg("Received PKT_FIRE (angle=%.4f)", g_PendingFire.angle);
    } else if (type == PKT_FLIGHT_CLICK && len == 13) {
        QueuedClick qc;
        qc.isDown = data[0];
        memcpy(&qc.clickCount, data + 1, 4);
        memcpy(&qc.x, data + 5, 4);
        memcpy(&qc.y, data + 9, 4);
        g_PendingClicks.push_back(qc);
    }
}

static void UrlEncodeParam(const char* src, char* dst, size_t cap) {
    static const char kHex[] = "0123456789ABCDEF";
    size_t w = 0;
    for (size_t i = 0; src && src[i] && w + 1 < cap; i++) {
        unsigned char c = (unsigned char)src[i];
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            dst[w++] = (char)c;
        } else if (w + 3 < cap) {
            dst[w++] = '%';
            dst[w++] = kHex[(c >> 4) & 0xf];
            dst[w++] = kHex[c & 0xf];
        } else {
            break;
        }
    }
    dst[w] = '\0';
}

static bool BuildRelayUrlComponents(
    const char* rawUrl,
    wchar_t* outHost,
    DWORD outHostChars,
    INTERNET_PORT* outPort,
    bool* outSecure,
    wchar_t* outBasePath,
    DWORD outBasePathChars
) {
    if (!rawUrl || !rawUrl[0]) return false;

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

    wchar_t pathBuf[256] = {};
    URL_COMPONENTSW uc = {};
    uc.dwStructSize = sizeof(uc);
    uc.lpszHostName = outHost;
    uc.dwHostNameLength = outHostChars;
    uc.lpszUrlPath = pathBuf;
    uc.dwUrlPathLength = 256;

    if (!WinHttpCrackUrl(wideUrl, 0, 0, &uc)) {
        return false;
    }

    *outPort = uc.nPort;
    *outSecure = (uc.nScheme == INTERNET_SCHEME_HTTPS);

    size_t plen = wcslen(pathBuf);
    while (plen > 0 && pathBuf[plen - 1] == L'/') {
        pathBuf[--plen] = L'\0';
    }
    wcsncpy(outBasePath, pathBuf, outBasePathChars - 1);
    outBasePath[outBasePathChars - 1] = L'\0';
    return outHost[0] != L'\0';
}

static void RunWebSocketSession() {
    wchar_t hostName[160] = {};
    wchar_t basePath[160] = {};
    INTERNET_PORT port = INTERNET_DEFAULT_HTTPS_PORT;
    bool isSecure = true;

    if (!BuildRelayUrlComponents(g_RelayUrl, hostName, 160, &port, &isSecure, basePath, 160)) {
        LogMsg("Invalid relay_url: '%s'", g_RelayUrl);
        Sleep(1500);
        return;
    }

    char encodedName[160] = {};
    UrlEncodeParam(g_PlayerName, encodedName, sizeof(encodedName));

    wchar_t wideRoom[64] = {};
    wchar_t wideName[160] = {};
    wchar_t wideLb[64] = {};
    MultiByteToWideChar(CP_ACP, 0, g_RoomCode, -1, wideRoom, 64);
    MultiByteToWideChar(CP_ACP, 0, encodedName, -1, wideName, 160);
    MultiByteToWideChar(CP_ACP, 0, g_LeaderboardCode, -1, wideLb, 64);

    wchar_t wsPath[512] = {};
    _snwprintf(
        wsPath, 512,
        L"%s/ws?room=%s&slot=%d&max=%d&name=%s&lb=%s",
        basePath, wideRoom, g_LocalPlayerSlot, g_ModePlayers, wideName, wideLb
    );
    wsPath[511] = L'\0';

    HINTERNET hSession = WinHttpOpen(
        L"PeggleOnline/2.0",
        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
    );
    if (!hSession) {
        LogMsg("WinHttpOpen failed (%u)", GetLastError());
        Sleep(1500);
        return;
    }

    WinHttpSetTimeouts(hSession, 10000, 15000, 15000, 45000);

    HINTERNET hConnect = WinHttpConnect(hSession, hostName, port, 0);
    if (!hConnect) {
        LogMsg("WinHttpConnect failed (%u)", GetLastError());
        WinHttpCloseHandle(hSession);
        Sleep(1500);
        return;
    }

    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect,
        L"GET",
        wsPath,
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        isSecure ? WINHTTP_FLAG_SECURE : 0
    );
    if (!hRequest) {
        LogMsg("WinHttpOpenRequest failed (%u)", GetLastError());
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        Sleep(1500);
        return;
    }

    if (!WinHttpSetOption(hRequest, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, nullptr, 0)) {
        LogMsg("WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET failed (%u)", GetLastError());
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        Sleep(1500);
        return;
    }

    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(hRequest, nullptr)) {
        LogMsg("WebSocket handshake failed (%u)", GetLastError());
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        Sleep(1500);
        return;
    }

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    if (!WinHttpQueryHeaders(
            hRequest,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &statusCode,
            &statusSize,
            WINHTTP_NO_HEADER_INDEX) ||
        statusCode != 101) {
        LogMsg("WebSocket upgrade rejected (HTTP %u)", statusCode);
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        Sleep(1500);
        return;
    }

    HINTERNET hWs = WinHttpWebSocketCompleteUpgrade(hRequest, 0);
    WinHttpCloseHandle(hRequest);
    hRequest = nullptr;

    if (!hWs) {
        LogMsg("WinHttpWebSocketCompleteUpgrade failed (%u)", GetLastError());
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        Sleep(1500);
        return;
    }

    DWORD infTimeout = 0;
    WinHttpSetOption(hWs, WINHTTP_OPTION_RECEIVE_TIMEOUT, &infTimeout, sizeof(infTimeout));
    WinHttpSetTimeouts(hWs, 0, 0, 0, 0);

    {
        std::lock_guard<std::mutex> lock(g_SendMutex);
        if (g_WantReconnect) {
            WinHttpCloseHandle(hWs);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return;
        }
        g_WsHandle = hWs;
        g_RelayOnline = true;
        g_RoomMaxPlayers = g_ModePlayers;
    }
    LogMsg("Connected to WebSocket relay room=%s slot=%d/%d name=%s lb=%s",
           g_RoomCode, g_LocalPlayerSlot, g_ModePlayers, g_PlayerName, g_LeaderboardCode);

    std::vector<uint8_t> rxChunk(65536);
    std::vector<uint8_t> msgBuf;
    constexpr size_t kMaxWsMsgBytes = 4 * 1024 * 1024;

    while (!g_WantReconnect) {
        DWORD bytesRead = 0;
        WINHTTP_WEB_SOCKET_BUFFER_TYPE bufType = WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE;
        DWORD err = WinHttpWebSocketReceive(
            hWs,
            rxChunk.data(),
            (DWORD)rxChunk.size(),
            &bytesRead,
            &bufType
        );
        if (err != NO_ERROR) {
            LogMsg("WinHttpWebSocketReceive ended (%u)", err);
            break;
        }
        if (bufType == WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE) {
            LogMsg("WebSocket closed by relay.");
            break;
        }
        if (bufType == WINHTTP_WEB_SOCKET_BINARY_FRAGMENT_BUFFER_TYPE ||
            bufType == WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE) {
            if (msgBuf.size() + bytesRead > kMaxWsMsgBytes) {
                LogMsg("Oversized WebSocket message rejected.");
                break;
            }
            msgBuf.insert(msgBuf.end(), rxChunk.data(), rxChunk.data() + bytesRead);

            if (bufType == WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE) {
                if (msgBuf.size() == 7 && memcmp(msgBuf.data(), "PGST", 4) == 0) {
                    g_RoomConnectedCount = (int)msgBuf[4];
                    g_RoomMaxPlayers = (int)msgBuf[5];
                } else if (msgBuf.size() == 4 && memcmp(msgBuf.data(), "PGOK", 4) == 0) {
                    {
                        std::lock_guard<std::mutex> lock(g_NetMutex);
                        g_Connected = true;
                        g_PeerJustJoined = true;
                    }
                    LogMsg("Room %s ready (%d/%d players -> PGOK)!", g_RoomCode, (int)g_RoomConnectedCount, (int)g_RoomMaxPlayers);
                    MessageBeep(MB_OK);
                } else if (msgBuf.size() == 4 && memcmp(msgBuf.data(), "PGBY", 4) == 0) {
                    g_Connected = false;
                    LogMsg("A player left room %s (PGBY).", g_RoomCode);
                } else if (msgBuf.size() >= 5) {
                    uint8_t type = msgBuf[0];
                    uint32_t plen = 0;
                    memcpy(&plen, msgBuf.data() + 1, 4);
                    if (5 + plen == msgBuf.size()) {
                        HandleIncomingPacket(type, msgBuf.data() + 5, plen);
                    }
                }
                msgBuf.clear();
            }
        }
    }

    HINTERNET handleToClose = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_SendMutex);
        if (g_WsHandle == hWs) {
            handleToClose = g_WsHandle;
            g_WsHandle = nullptr;
        }
    }
    g_Connected = false;
    g_RelayOnline = false;
    g_RoomConnectedCount = 0;
    if (handleToClose) {
        WinHttpCloseHandle(handleToClose);
    }
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    LogMsg("WebSocket session ended.");
    Sleep(1500);
}

static DWORD WINAPI NetworkThreadProc(LPVOID) {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    while (true) {
        g_WantReconnect = false;

        if (g_Role == ROLE_WS_HOST || g_Role == ROLE_WS_CLIENT) {
            RunWebSocketSession();
            continue;
        }

        SOCKET peer = INVALID_SOCKET;

        if (g_Role == ROLE_HOST) {
            SOCKET listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (listenSock == INVALID_SOCKET) { Sleep(1000); continue; }
            int opt = 1;
            setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

            sockaddr_in addr = {};
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = INADDR_ANY;
            addr.sin_port = htons((u_short)g_Port);

            if (bind(listenSock, (sockaddr*)&addr, sizeof(addr)) != 0 || listen(listenSock, 1) != 0) {
                LogMsg("Host bind/listen failed on port %d", g_Port);
                closesocket(listenSock);
                Sleep(1500);
                continue;
            }
            LogMsg("Host listening on port %d...", g_Port);

            while (!g_WantReconnect) {
                fd_set rfds;
                FD_ZERO(&rfds);
                FD_SET(listenSock, &rfds);
                timeval tv = { 0, 250000 };
                int r = select(0, &rfds, nullptr, nullptr, &tv);
                if (r > 0) {
                    peer = accept(listenSock, nullptr, nullptr);
                    break;
                }
            }
            closesocket(listenSock);
        } else {
            while (!g_WantReconnect) {
                SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
                sockaddr_in addr = {};
                addr.sin_family = AF_INET;
                addr.sin_port = htons((u_short)g_Port);
                inet_pton(AF_INET, g_HostIp, &addr.sin_addr);

                if (connect(s, (sockaddr*)&addr, sizeof(addr)) == 0) {
                    if (g_Role == ROLE_RELAY_HOST || g_Role == ROLE_RELAY_CLIENT) {
                        const char* tag = (g_Role == ROLE_RELAY_HOST) ? "PGH1" : "PGC2";
                        SendAll(s, tag, 4);
                        char ack[4] = {};
                        if (!RecvAll(s, ack, 4) || memcmp(ack, "PGOK", 4) != 0) {
                            closesocket(s);
                            Sleep(1000);
                            continue;
                        }
                    }
                    peer = s;
                    break;
                }
                closesocket(s);
                Sleep(1000);
            }
        }

        if (peer == INVALID_SOCKET || g_WantReconnect) {
            if (peer != INVALID_SOCKET) closesocket(peer);
            continue;
        }

        int nodelay = 1;
        setsockopt(peer, IPPROTO_TCP, TCP_NODELAY, (const char*)&nodelay, sizeof(nodelay));

        {
            std::lock_guard<std::mutex> lock(g_SendMutex);
            g_PeerSock = peer;
            g_Connected = true;
            g_PeerJustJoined = true;
        }
        LogMsg("Peer connected!");
        MessageBeep(MB_OK);

        while (!g_WantReconnect) {
            uint8_t hdr[5];
            if (!RecvAll(peer, hdr, 5)) break;
            uint8_t type = hdr[0];
            uint32_t len = 0;
            memcpy(&len, hdr + 1, 4);
            if (len > 16 * 1024 * 1024) break;

            std::vector<uint8_t> buf(len);
            if (len > 0 && !RecvAll(peer, buf.data(), (int)len)) break;

            HandleIncomingPacket(type, buf.data(), len);
        }

        {
            std::lock_guard<std::mutex> lock(g_SendMutex);
            if (g_PeerSock == peer) {
                closesocket(g_PeerSock);
                g_PeerSock = INVALID_SOCKET;
            }
            g_Connected = false;
        }
        LogMsg("Peer disconnected.");
        Sleep(500);
    }
    return 0;
}

static DWORD WINAPI HudThreadProc(LPVOID) {
    while (true) {
        Sleep(250);
        if (g_ModEnabled) {
            TickMenuTitleAndHotkeys();
        }
    }
    return 0;
}

// ============================================================================
// 4. VTable Hooks on Sexy::Board (0x6864a4) & Sexy::ThunderballApp (0x68670c)
// ============================================================================
typedef void (__thiscall *FnBoardUpdate)(void* self);
typedef void (__thiscall *FnBoardMouseMove)(void* self, int x, int y);
typedef void (__thiscall *FnBoardMouseDown)(void* self, int x, int y, int clickCount);
typedef void (__thiscall *FnBoardMouseUp)(void* self, int x, int y, int clickCount);
typedef void (__thiscall *FnBoardMouseDrag)(void* self, int x, int y);
typedef void (__thiscall *FnBoardMouseWheel)(void* self, int delta);
typedef void (__thiscall *FnBoardKeyDown)(void* self, int keyCode);
typedef void (__thiscall *FnBoardKeyUp)(void* self, int keyCode);
typedef void (__thiscall *FnAppFocus)(void* self);

static FnBoardUpdate     Orig_Board_Update     = nullptr;
static FnBoardMouseMove  Orig_Board_MouseMove  = nullptr;
static FnBoardMouseDown  Orig_Board_MouseDown  = nullptr;
static FnBoardMouseUp    Orig_Board_MouseUp    = nullptr;
static FnBoardMouseDrag  Orig_Board_MouseDrag  = nullptr;
static FnBoardMouseWheel Orig_Board_MouseWheel = nullptr;
static FnBoardKeyDown    Orig_Board_KeyDown    = nullptr;
static FnBoardKeyUp      Orig_Board_KeyUp      = nullptr;
static FnAppFocus        Orig_App_GotFocus     = nullptr;
static FnAppFocus        Orig_App_LostFocus    = nullptr;

static bool     g_AllowRemoteInput  = false;
static void*    g_LastBoard         = nullptr;
static uint8_t  g_PrevCharDlgOpen   = 1;
static int      g_PrevLogicState    = 0;
static bool     g_ShotSentThisTurn  = false;
static bool     g_LocalWasShooting  = false;
static bool     g_MatchBallsApplied = false;
static bool     g_MatchResultSent   = false;
static int      g_MatchSeq          = 0;
static float    g_LastSentAngle     = -999.0f;
static DWORD    g_LastAimSendMs     = 0;
static DWORD    g_LastTitleMs       = 0;

static int GetActivePlayerSlot(void* logic) {
    if (!logic) return 0;
    int curTeam = *(int*)((char*)logic + 0x1ac); // 0 = Team A (Left), 1 = Team B (Right)
    if (g_ModePlayers <= 2) return curTeam;
    int state = *(int*)((char*)logic + 0x4);
    int teamShots = *(int*)((char*)logic + 0x2dc + curTeam * 0x7c);
    int idx = (state == 1) ? teamShots : (teamShots > 0 ? teamShots - 1 : 0);
    return (idx % 2 == 0) ? curTeam : (curTeam + 2);
}

static bool IsOnlineDuelActive(void* board, void** outLogic = nullptr, int* outActiveSlot = nullptr) {
    if (!g_ModEnabled || !g_Connected || !board) return false;
    int gameMode = *(int*)((char*)board + 0xb4);
    uint8_t charDlgOpen = *(uint8_t*)((char*)board + 0x106);
    void* logic = *(void**)((char*)board + 0x720);
    if (!logic) return false;
    uint8_t isReplay = *(uint8_t*)((char*)logic + 0x101);
    if (gameMode != 3 || charDlgOpen != 0 || isReplay != 0) return false;

    if (outLogic) *outLogic = logic;
    if (outActiveSlot) *outActiveSlot = GetActivePlayerSlot(logic);
    return true;
}

static bool IsRemoteTurn(void* board) {
    if (g_AllowRemoteInput) return false;
    int activeSlot = 0;
    if (!IsOnlineDuelActive(board, nullptr, &activeSlot)) return false;
    return activeSlot != g_LocalPlayerSlot;
}

static void BroadcastBoardStateSync(void* board, bool isInitial) {
    if (!board || !g_Connected) return;
    std::vector<uint8_t> boardBytes;
    if (!Engine_Board_SaveState(board, boardBytes)) return;

    uint32_t stateLen = (uint32_t)boardBytes.size();
    std::vector<uint8_t> payload(1 + MT_STATE_BYTES + 4 + stateLen);
    payload[0] = isInitial ? 1 : 0;
    memcpy(payload.data() + 1, (const void*)ADDR_MT_STATE, MT_STATE_BYTES);
    memcpy(payload.data() + 1 + MT_STATE_BYTES, &stateLen, 4);
    memcpy(payload.data() + 1 + MT_STATE_BYTES + 4, boardBytes.data(), stateLen);

    SendPacket(PKT_STATE_SYNC, payload.data(), (uint32_t)payload.size());
    LogMsg("Sent PKT_STATE_SYNC (initial=%d, bytes=%u)", (int)isInitial, stateLen);
}

static void SubmitLeaderboardResult(int scoreA, int scoreB) {
    if (g_LeaderboardCode[0] == '\0') return;
    g_MatchSeq++;
    const char* modeStr = (g_ModePlayers == 4) ? "2v2" : "1v1";
    int effectiveBalls = (g_BallsPerPlayer > 0) ? g_BallsPerPlayer : 6;
    char json[512];
    snprintf(
        json, sizeof(json),
        "{\"leaderboard\":\"%s\",\"matchId\":\"%s_%u_%d\",\"mode\":\"%s\",\"ballsPerPlayer\":%d,\"scoreA\":%d,\"scoreB\":%d}",
        g_LeaderboardCode, g_RoomCode, (unsigned int)GetTickCount(), g_MatchSeq,
        modeStr, effectiveBalls, scoreA, scoreB
    );
    SendPacket(PKT_MATCH_RESULT, json, (uint32_t)strlen(json));
    LogMsg("Submitted PKT_MATCH_RESULT to leaderboard %s: A=%d vs B=%d (%s)",
           g_LeaderboardCode, scoreA, scoreB, modeStr);
}

static void UpdateWindowTitleHUD(void* board) {
    std::lock_guard<std::mutex> lock(g_HudMutex);
    DWORD now = GetTickCount();
    if (now - g_LastTitleMs < 250) return;
    g_LastTitleMs = now;

    void* app = *(void**)ADDR_G_SEXY_APP_BASE;
    if (!app) return;
    HWND hwnd = *(HWND*)((char*)app + 0x350);
    if (!hwnd || !IsWindow(hwnd)) return;

    bool isWs = (g_Role == ROLE_WS_HOST || g_Role == ROLE_WS_CLIENT);
    const char* modeLabel = (g_ModePlayers == 4) ? "2v2" : "1v1";
    char slotLabel[32];
    if (g_ModePlayers == 4) {
        snprintf(slotLabel, sizeof(slotLabel), "P%d Team %c",
                 g_LocalPlayerSlot + 1, (g_LocalPlayerSlot % 2 == 0) ? 'A' : 'B');
    } else {
        snprintf(slotLabel, sizeof(slotLabel), "%s",
                 (g_LocalPlayerSlot == 0) ? "P1 Host" : "P2 Join");
    }

    char title[256];
    if (!g_Connected) {
        if (isWs) {
            if (g_RelayOnline) {
                int cnt = (int)g_RoomConnectedCount;
                int maxP = (int)g_RoomMaxPlayers;
                snprintf(title, sizeof(title),
                    "Peggle Nights | %s [Room: %s | %s] Waiting (%d/%d players)... (F6:Slot F7:Retry)",
                    modeLabel, g_RoomCode, slotLabel, cnt, maxP);
            } else {
                snprintf(title, sizeof(title),
                    "Peggle Nights | %s [Room: %s | %s] Connecting to server... (F6:Slot F7:Retry)",
                    modeLabel, g_RoomCode, slotLabel);
            }
        } else if (g_Role == ROLE_HOST) {
            snprintf(title, sizeof(title),
                "Peggle Nights | ONLINE DUEL [P1 (HOST)] Waiting on port %d... (F6:Switch Role F7:Reset)",
                g_Port);
        } else {
            snprintf(title, sizeof(title),
                "Peggle Nights | ONLINE DUEL [P2 (CLIENT)] Connecting to %s:%d... (F6:Switch Role F7:Reset)",
                g_HostIp, g_Port);
        }
        SetWindowTextA(hwnd, title);
        return;
    }

    void* logic = board ? *(void**)((char*)board + 0x720) : nullptr;
    int gameMode = board ? *(int*)((char*)board + 0xb4) : 0;
    uint8_t charDlg = board ? *(uint8_t*)((char*)board + 0x106) : 1;

    if (board && logic && gameMode == 3 && charDlg == 0) {
        int state     = *(int*)((char*)logic + 0x4);
        int scoreA    = *(int*)((char*)logic + 0x200);
        int scoreB    = *(int*)((char*)logic + 0x204);
        int ballsA    = *(int*)((char*)logic + 0x208);
        int ballsB    = *(int*)((char*)logic + 0x20c);

        if (state == 5) {
            if (g_LeaderboardCode[0]) {
                snprintf(title, sizeof(title),
                    "Peggle Nights | %s [Room: %s | %s] MATCH OVER! A %d vs B %d | Saved to LB: %s",
                    modeLabel, g_RoomCode, slotLabel, scoreA, scoreB, g_LeaderboardCode);
            } else {
                snprintf(title, sizeof(title),
                    "Peggle Nights | %s [Room: %s | %s] MATCH OVER! Team A %d vs Team B %d",
                    modeLabel, g_RoomCode, slotLabel, scoreA, scoreB);
            }
        } else {
            int activeSlot = GetActivePlayerSlot(logic);
            char turnStr[64];
            if (activeSlot == g_LocalPlayerSlot) {
                snprintf(turnStr, sizeof(turnStr), "YOUR TURN - AIM & FIRE!");
            } else if (g_ModePlayers == 4) {
                snprintf(turnStr, sizeof(turnStr), "P%d'S TURN (Team %c)",
                         activeSlot + 1, (activeSlot % 2 == 0) ? 'A' : 'B');
            } else {
                snprintf(turnStr, sizeof(turnStr), "FRIEND'S TURN (Watching)");
            }

            if (isWs) {
                snprintf(title, sizeof(title),
                    "Peggle Nights | %s [Room: %s | %s] %s | A:%d(%d) vs B:%d(%d) (F5:Sync)",
                    modeLabel, g_RoomCode, slotLabel, turnStr, scoreA, ballsA, scoreB, ballsB);
            } else {
                snprintf(title, sizeof(title),
                    "Peggle Nights | ONLINE DUEL [%s] %s | P1 %d vs P2 %d (F5:Sync)",
                    slotLabel, turnStr, scoreA, scoreB);
            }
        }
    } else {
        if (isWs) {
            snprintf(title, sizeof(title),
                "Peggle Nights | %s [Room: %s | %s] ALL %d READY! Enter Duel -> PLAY A FRIEND -> PLAY NOW!",
                modeLabel, g_RoomCode, slotLabel, g_ModePlayers);
        } else {
            snprintf(title, sizeof(title),
                "Peggle Nights | ONLINE DUEL [%s] CONNECTED! Enter Duel Mode & Click Start Game!",
                slotLabel);
        }
    }
    SetWindowTextA(hwnd, title);
}

static void PollHotkeys(void* board) {
    std::lock_guard<std::mutex> lock(g_HudMutex);
    static bool f5Down = false, f6Down = false, f7Down = false;
    bool f5 = (GetAsyncKeyState(VK_F5) & 0x8000) != 0;
    bool f6 = (GetAsyncKeyState(VK_F6) & 0x8000) != 0;
    bool f7 = (GetAsyncKeyState(VK_F7) & 0x8000) != 0;

    if (f5 && !f5Down && board) {
        BroadcastBoardStateSync(board, true);
        MessageBeep(MB_ICONINFORMATION);
    }
    if (f6 && !f6Down) {
        if (g_Role == ROLE_WS_HOST || g_Role == ROLE_WS_CLIENT) {
            g_LocalPlayerSlot = (g_LocalPlayerSlot + 1) % g_ModePlayers;
            g_LocalPlayerIdx = g_LocalPlayerSlot;
            g_Role = (g_LocalPlayerSlot == 0) ? ROLE_WS_HOST : ROLE_WS_CLIENT;
        } else if (g_Role == ROLE_HOST) {
            g_Role = ROLE_CLIENT; g_LocalPlayerSlot = 1; g_LocalPlayerIdx = 1;
        } else if (g_Role == ROLE_CLIENT) {
            g_Role = ROLE_HOST; g_LocalPlayerSlot = 0; g_LocalPlayerIdx = 0;
        } else if (g_Role == ROLE_RELAY_HOST) {
            g_Role = ROLE_RELAY_CLIENT; g_LocalPlayerSlot = 1; g_LocalPlayerIdx = 1;
        } else {
            g_Role = ROLE_RELAY_HOST; g_LocalPlayerSlot = 0; g_LocalPlayerIdx = 0;
        }
        g_WantReconnect = true;
        CloseActiveTransport();
        MessageBeep(MB_ICONWARNING);
    }
    if (f7 && !f7Down) {
        g_WantReconnect = true;
        CloseActiveTransport();
        MessageBeep(MB_ICONWARNING);
    }
    f5Down = f5; f6Down = f6; f7Down = f7;
}

static void __fastcall Hook_Board_Update(void* board, void* /*edx*/) {
    PollHotkeys(board);
    UpdateWindowTitleHUD(board);

    if (board != g_LastBoard) {
        g_LastBoard = board;
        g_PrevCharDlgOpen = 1;
        g_PrevLogicState = 0;
        g_ShotSentThisTurn = false;
        g_LocalWasShooting = false;
        g_MatchBallsApplied = false;
        g_MatchResultSent = false;
    }

    void* logic = *(void**)((char*)board + 0x720);
    void* gun   = *(void**)((char*)board + 0xd4);
    int gameMode = *(int*)((char*)board + 0xb4);
    uint8_t charDlgOpen = *(uint8_t*)((char*)board + 0x106);

    if (g_ModEnabled && g_Connected && logic && gameMode == 3) {
        // Always force Duel mode to Human vs Human (disable Computer AI for both sides)
        *(uint8_t*)((char*)logic + 0x1a8) = 1; // mIsDuel = 1
        *(uint8_t*)((char*)logic + 0x2d8) = 0; // mIsComputer[0] = 0
        *(uint8_t*)((char*)logic + 0x2d9) = 0; // mIsComputer[1] = 0

        int state = *(int*)((char*)logic + 0x4);
        int shot0 = *(int*)((char*)logic + 0x2dc);
        int shot1 = *(int*)((char*)logic + 0x358);

        // 1. Host applies custom balls per player & broadcasts initial board state
        bool peerJoined = false;
        {
            std::lock_guard<std::mutex> lock(g_NetMutex);
            if (g_PeerJustJoined) {
                peerJoined = true;
                g_PeerJustJoined = false;
            }
        }

        if (g_LocalPlayerSlot == 0 && charDlgOpen == 0 && state == 1) {
            if (shot0 == 0 && shot1 == 0) {
                g_MatchResultSent = false;
                if (!g_MatchBallsApplied && g_BallsPerPlayer > 0) {
                    int teamBalls = (g_ModePlayers == 4) ? (g_BallsPerPlayer * 2) : g_BallsPerPlayer;
                    *(int*)((char*)logic + 0x208) = teamBalls;
                    *(int*)((char*)logic + 0x20c) = teamBalls;
                    Engine_Interface_SetNumBalls(*(void**)((char*)board + 0x718), teamBalls);
                    g_MatchBallsApplied = true;
                    LogMsg("Applied custom ball count: %d/player (%d/team)", g_BallsPerPlayer, teamBalls);
                }
            }
            if (g_PrevCharDlgOpen != 0 || peerJoined) {
                BroadcastBoardStateSync(board, true);
                g_ShotSentThisTurn = false;
            }
        }
        g_PrevCharDlgOpen = charDlgOpen;

        // 2. Host submits match result once when entering End Level (state == 5)
        if (g_LocalPlayerSlot == 0 && charDlgOpen == 0 && state == 5 &&
            !g_MatchResultSent && (shot0 + shot1) > 0) {
            int scoreA = *(int*)((char*)logic + 0x200);
            int scoreB = *(int*)((char*)logic + 0x204);
            SubmitLeaderboardResult(scoreA, scoreB);
            g_MatchResultSent = true;
        }

        // 3. Check if our own shot just finished and transitioned back to State 1 (next turn!)
        if (charDlgOpen == 0 && state == 1 && g_PrevLogicState != 1) {
            g_ShotSentThisTurn = false;
            if (g_LocalWasShooting) {
                g_LocalWasShooting = false;
                BroadcastBoardStateSync(board, false);
            }
        }
        g_PrevLogicState = state;

        // 4. Apply incoming PKT_STATE_SYNC if ready
        if (charDlgOpen == 0) {
            QueuedStateSync syncToApply;
            {
                std::lock_guard<std::mutex> lock(g_NetMutex);
                if (g_PendingSync.valid) {
                    bool ready = g_PendingSync.isInitial || (state == 1) ||
                                 (GetTickCount() - g_PendingSync.receivedTick > 2000);
                    if (ready) {
                        syncToApply = std::move(g_PendingSync);
                        g_PendingSync.valid = false;
                    }
                }
            }
            if (syncToApply.valid) {
                memcpy((void*)ADDR_MT_STATE, syncToApply.mtState, MT_STATE_BYTES);
                Engine_Board_LoadState(board, syncToApply.boardBytes.data(), (uint32_t)syncToApply.boardBytes.size());
                logic = *(void**)((char*)board + 0x720);
                gun   = *(void**)((char*)board + 0xd4);
                if (logic) {
                    *(uint8_t*)((char*)logic + 0x1a8) = 1;
                    *(uint8_t*)((char*)logic + 0x2d8) = 0;
                    *(uint8_t*)((char*)logic + 0x2d9) = 0;
                    state = *(int*)((char*)logic + 0x4);
                    int curTeam = *(int*)((char*)logic + 0x1ac);
                    Engine_Interface_SetNumBalls(
                        *(void**)((char*)board + 0x718),
                        *(int*)((char*)logic + 0x208 + curTeam * 4)
                    );
                }
                g_ShotSentThisTurn = false;
                g_LocalWasShooting = false;
                LogMsg("Applied PKT_STATE_SYNC (activeSlot=%d, state=%d)", GetActivePlayerSlot(logic), state);
            }
        }

        // 5. Local Player's Turn vs Remote Player's Turn
        if (charDlgOpen == 0 && logic) {
            int activeSlot = GetActivePlayerSlot(logic);
            if (activeSlot == g_LocalPlayerSlot) {
                // LOCAL TURN
                if (state == 1) {
                    uint8_t wantFire = *(uint8_t*)((char*)logic + 0xf9);
                    uint8_t fireNow  = *(uint8_t*)((char*)logic + 0xf8);
                    if ((wantFire != 0 || fireNow != 0) && !g_ShotSentThisTurn) {
                        if (*(uint8_t*)((char*)logic + 0x20) != 0) {
                            Engine_Logic_CalcMouseAim(logic);
                        }
                        float fireAngle = *(float*)((char*)logic + 0xec);
                        Engine_Gun_SetAngle(gun, fireAngle, 1);

                        uint8_t pkt[4 + MT_STATE_BYTES];
                        memcpy(pkt, &fireAngle, 4);
                        memcpy(pkt + 4, (const void*)ADDR_MT_STATE, MT_STATE_BYTES);
                        SendPacket(PKT_FIRE, pkt, sizeof(pkt));

                        g_ShotSentThisTurn = true;
                        g_LocalWasShooting = true;
                        LogMsg("Sent PKT_FIRE (slot=%d, angle=%.4f)", g_LocalPlayerSlot, fireAngle);
                    } else if (!g_ShotSentThisTurn) {
                        float curAngle = *(float*)((char*)logic + 0xec);
                        DWORD now = GetTickCount();
                        if (fabsf(curAngle - g_LastSentAngle) > 0.002f && (now - g_LastAimSendMs >= 40)) {
                            SendPacket(PKT_AIM, &curAngle, 4);
                            g_LastSentAngle = curAngle;
                            g_LastAimSendMs = now;
                        }
                    }
                } else {
                    g_LocalWasShooting = true;
                }
            } else {
                // REMOTE TURN (Watching)
                *(uint8_t*)((char*)logic + 0x20) = 0;

                QueuedFire fireToApply;
                bool hasAim = false;
                float aimAngle = 0.0f;
                std::vector<QueuedClick> clicksToApply;
                {
                    std::lock_guard<std::mutex> lock(g_NetMutex);
                    if (g_PendingFire.valid && state == 1) {
                        fireToApply = g_PendingFire;
                        g_PendingFire.valid = false;
                    }
                    if (g_HasRemoteAim) {
                        hasAim = true;
                        aimAngle = g_RemoteAimAngle;
                        g_HasRemoteAim = false;
                    }
                    if (!g_PendingClicks.empty()) {
                        clicksToApply.swap(g_PendingClicks);
                    }
                }

                if (hasAim && state == 1 && !fireToApply.valid) {
                    *(float*)((char*)logic + 0xec) = aimAngle;
                    Engine_Gun_SetAngle(gun, aimAngle, 0);
                }

                if (fireToApply.valid && state == 1) {
                    memcpy((void*)ADDR_MT_STATE, fireToApply.mtState, MT_STATE_BYTES);
                    *(uint8_t*)((char*)logic + 0x20) = 0;
                    *(float*)((char*)logic + 0xec) = fireToApply.angle;
                    Engine_Gun_SetAngle(gun, fireToApply.angle, 1);
                    *(uint8_t*)((char*)logic + 0xf9) = 1;
                    LogMsg("Applied PKT_FIRE (from slot=%d, angle=%.4f)", activeSlot, fireToApply.angle);
                }

                if (!clicksToApply.empty()) {
                    g_AllowRemoteInput = true;
                    for (const auto& qc : clicksToApply) {
                        if (qc.isDown) {
                            Orig_Board_MouseDown(board, qc.x, qc.y, qc.clickCount);
                        } else {
                            Orig_Board_MouseUp(board, qc.x, qc.y, qc.clickCount);
                        }
                    }
                    g_AllowRemoteInput = false;
                }
            }
        }
    }

    Orig_Board_Update(board);
}

static void __fastcall Hook_Board_MouseMove(void* board, void* /*edx*/, int x, int y) {
    if (IsRemoteTurn(board)) return;
    Orig_Board_MouseMove(board, x, y);
}

static void __fastcall Hook_Board_MouseDrag(void* board, void* /*edx*/, int x, int y) {
    if (IsRemoteTurn(board)) return;
    Orig_Board_MouseDrag(board, x, y);
}

static void __fastcall Hook_Board_MouseWheel(void* board, void* /*edx*/, int delta) {
    if (IsRemoteTurn(board)) return;
    Orig_Board_MouseWheel(board, delta);
}

static void __fastcall Hook_Board_MouseDown(void* board, void* /*edx*/, int x, int y, int clickCount) {
    if (IsRemoteTurn(board)) return;

    void* logic = nullptr;
    int activeSlot = 0;
    bool onlineActive = IsOnlineDuelActive(board, &logic, &activeSlot);
    int stateBefore = (onlineActive && logic) ? *(int*)((char*)logic + 0x4) : 0;

    Orig_Board_MouseDown(board, x, y, clickCount);

    if (onlineActive && !g_AllowRemoteInput && activeSlot == g_LocalPlayerSlot && stateBefore == 2) {
        uint8_t pkt[13];
        pkt[0] = 1;
        memcpy(pkt + 1, &clickCount, 4);
        memcpy(pkt + 5, &x, 4);
        memcpy(pkt + 9, &y, 4);
        SendPacket(PKT_FLIGHT_CLICK, pkt, sizeof(pkt));
    }
}

static void __fastcall Hook_Board_MouseUp(void* board, void* /*edx*/, int x, int y, int clickCount) {
    if (IsRemoteTurn(board)) return;

    void* logic = nullptr;
    int activeSlot = 0;
    bool onlineActive = IsOnlineDuelActive(board, &logic, &activeSlot);
    int stateBefore = (onlineActive && logic) ? *(int*)((char*)logic + 0x4) : 0;

    Orig_Board_MouseUp(board, x, y, clickCount);

    if (onlineActive && !g_AllowRemoteInput && activeSlot == g_LocalPlayerSlot && stateBefore == 2) {
        uint8_t pkt[13];
        pkt[0] = 0;
        memcpy(pkt + 1, &clickCount, 4);
        memcpy(pkt + 5, &x, 4);
        memcpy(pkt + 9, &y, 4);
        SendPacket(PKT_FLIGHT_CLICK, pkt, sizeof(pkt));
    }
}

static void __fastcall Hook_Board_KeyDown(void* board, void* /*edx*/, int keyCode) {
    if (IsRemoteTurn(board)) return;
    Orig_Board_KeyDown(board, keyCode);
}

static void __fastcall Hook_Board_KeyUp(void* board, void* /*edx*/, int keyCode) {
    if (IsRemoteTurn(board)) return;
    Orig_Board_KeyUp(board, keyCode);
}

static void __fastcall Hook_App_GotFocus(void* app, void* /*edx*/) {
    if (g_ModEnabled) return;
    Orig_App_GotFocus(app);
}

static void __fastcall Hook_App_LostFocus(void* app, void* /*edx*/) {
    if (g_ModEnabled) return;
    Orig_App_LostFocus(app);
}

// ============================================================================
// 5. Initialization & VTable Patching
// ============================================================================
static void NormalizeRoomCode(char* buf, const char* fallback) {
    size_t w = 0;
    for (size_t r = 0; buf[r] != '\0' && w < 12; r++) {
        unsigned char c = (unsigned char)buf[r];
        if (isalnum(c)) {
            buf[w++] = (char)toupper(c);
        }
    }
    buf[w] = '\0';
    if (w < 2 && fallback) {
        strcpy(buf, fallback);
    }
}

static void SanitizeLbCode(char* buf) {
    size_t w = 0;
    for (size_t r = 0; buf[r] != '\0' && w < 16; r++) {
        unsigned char c = (unsigned char)buf[r];
        if (isalnum(c) || c == '-') {
            buf[w++] = (char)toupper(c);
        }
    }
    buf[w] = '\0';
    if (w < 2) buf[0] = '\0';
}

static void SanitizeDisplayName(char* buf) {
    size_t w = 0;
    for (size_t r = 0; buf[r] != '\0' && w < 24; r++) {
        unsigned char c = (unsigned char)buf[r];
        if (c >= 0x20 && c != 0x7f && c != '"' && c != '\\') {
            buf[w++] = (char)c;
        }
    }
    buf[w] = '\0';
    while (w > 0 && isspace((unsigned char)buf[w - 1])) buf[--w] = '\0';
    if (w == 0) strcpy(buf, "Player");
}

static void LoadConfig(const char* iniPath) {
    g_ModEnabled = GetPrivateProfileIntA("Online", "enabled", 1, iniPath) != 0;
    g_Port       = GetPrivateProfileIntA("Online", "port", 27888, iniPath);

    int modeP = GetPrivateProfileIntA("Online", "mode_players", 2, iniPath);
    g_ModePlayers = (modeP == 4) ? 4 : 2;

    int bpp = GetPrivateProfileIntA("Online", "balls_per_player", 0, iniPath);
    if (bpp < 0) bpp = 0;
    if (bpp > 6) bpp = 6;
    g_BallsPerPlayer = bpp;

    GetPrivateProfileStringA("Online", "player_name", "Player", g_PlayerName, sizeof(g_PlayerName), iniPath);
    SanitizeDisplayName(g_PlayerName);

    GetPrivateProfileStringA("Online", "leaderboard_code", "", g_LeaderboardCode, sizeof(g_LeaderboardCode), iniPath);
    SanitizeLbCode(g_LeaderboardCode);

    GetPrivateProfileStringA("Online", "host_ip", "127.0.0.1", g_HostIp, sizeof(g_HostIp), iniPath);
    GetPrivateProfileStringA("Online", "relay_url", "", g_RelayUrl, sizeof(g_RelayUrl), iniPath);
    GetPrivateProfileStringA("Online", "room_code", "PEGG", g_RoomCode, sizeof(g_RoomCode), iniPath);
    NormalizeRoomCode(g_RoomCode, "PEGG");

    char roleBuf[64] = {};
    GetPrivateProfileStringA("Online", "role", "host", roleBuf, sizeof(roleBuf), iniPath);
    int defaultSlot = 0;
    if (_stricmp(roleBuf, "client") == 0) {
        g_Role = ROLE_CLIENT;
        defaultSlot = 1;
    } else if (_stricmp(roleBuf, "relay_host") == 0) {
        g_Role = ROLE_RELAY_HOST;
        defaultSlot = 0;
    } else if (_stricmp(roleBuf, "relay_client") == 0) {
        g_Role = ROLE_RELAY_CLIENT;
        defaultSlot = 1;
    } else if (_stricmp(roleBuf, "ws_host") == 0) {
        g_Role = ROLE_WS_HOST;
        defaultSlot = 0;
    } else if (_stricmp(roleBuf, "ws_client") == 0) {
        g_Role = ROLE_WS_CLIENT;
        defaultSlot = 1;
    } else {
        g_Role = ROLE_HOST;
        defaultSlot = 0;
    }

    int slot = GetPrivateProfileIntA("Online", "player_slot", -1, iniPath);
    if (slot >= 0 && slot < g_ModePlayers) {
        g_LocalPlayerSlot = slot;
    } else {
        g_LocalPlayerSlot = defaultSlot;
    }
    g_LocalPlayerIdx = g_LocalPlayerSlot;
    if (g_Role == ROLE_WS_HOST || g_Role == ROLE_WS_CLIENT) {
        g_Role = (g_LocalPlayerSlot == 0) ? ROLE_WS_HOST : ROLE_WS_CLIENT;
    }
}

static void InstallHooksIfInnerGame() {
    __try {
        uintptr_t* boardVtbl = (uintptr_t*)ADDR_BOARD_VTABLE;
        uintptr_t* appVtbl   = (uintptr_t*)ADDR_APP_VTABLE;
        if (boardVtbl[0] != 0x00404850 || boardVtbl[22] != 0x0040e460) {
            return;
        }

        char logPath[MAX_PATH];
        snprintf(logPath, sizeof(logPath), "%speggle_online.log", g_ModDir);
        g_LogFile = fopen(logPath, "a+");

        char iniPath[MAX_PATH];
        snprintf(iniPath, sizeof(iniPath), "%speggle_online.ini", g_ModDir);
        LoadConfig(iniPath);

        LogMsg("=== %s Loaded (enabled=%d, mode=%d, slot=%d, balls=%d, name=%s, lb=%s, room=%s) ===",
               kModMarker, (int)g_ModEnabled, g_ModePlayers, g_LocalPlayerSlot,
               g_BallsPerPlayer, g_PlayerName, g_LeaderboardCode, g_RoomCode);

        if (!g_ModEnabled) return;

        DWORD oldProt = 0;
        VirtualProtect(boardVtbl, 70 * sizeof(uintptr_t), PAGE_EXECUTE_READWRITE, &oldProt);
        Orig_Board_Update     = (FnBoardUpdate)boardVtbl[22];
        Orig_Board_KeyDown    = (FnBoardKeyDown)boardVtbl[48];
        Orig_Board_KeyUp      = (FnBoardKeyUp)boardVtbl[49];
        Orig_Board_MouseMove  = (FnBoardMouseMove)boardVtbl[52];
        Orig_Board_MouseDown  = (FnBoardMouseDown)boardVtbl[54];
        Orig_Board_MouseUp    = (FnBoardMouseUp)boardVtbl[56];
        Orig_Board_MouseDrag  = (FnBoardMouseDrag)boardVtbl[58];
        Orig_Board_MouseWheel = (FnBoardMouseWheel)boardVtbl[59];

        boardVtbl[22] = (uintptr_t)&Hook_Board_Update;
        boardVtbl[48] = (uintptr_t)&Hook_Board_KeyDown;
        boardVtbl[49] = (uintptr_t)&Hook_Board_KeyUp;
        boardVtbl[52] = (uintptr_t)&Hook_Board_MouseMove;
        boardVtbl[54] = (uintptr_t)&Hook_Board_MouseDown;
        boardVtbl[56] = (uintptr_t)&Hook_Board_MouseUp;
        boardVtbl[58] = (uintptr_t)&Hook_Board_MouseDrag;
        boardVtbl[59] = (uintptr_t)&Hook_Board_MouseWheel;
        VirtualProtect(boardVtbl, 70 * sizeof(uintptr_t), oldProt, &oldProt);

        VirtualProtect(appVtbl, 108 * sizeof(uintptr_t), PAGE_EXECUTE_READWRITE, &oldProt);
        Orig_App_GotFocus  = (FnAppFocus)appVtbl[84];
        Orig_App_LostFocus = (FnAppFocus)appVtbl[85];
        appVtbl[84] = (uintptr_t)&Hook_App_GotFocus;
        appVtbl[85] = (uintptr_t)&Hook_App_LostFocus;
        VirtualProtect(appVtbl, 108 * sizeof(uintptr_t), oldProt, &oldProt);

        CreateThread(nullptr, 0, NetworkThreadProc, nullptr, 0, nullptr);
        CreateThread(nullptr, 0, HudThreadProc, nullptr, 0, nullptr);
        LogMsg("Hooks installed, network & HUD threads started.");
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        // Not the target inner game process
    }
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID /*lpReserved*/) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);

        GetModuleFileNameA(hModule, g_ModDir, MAX_PATH);
        char* lastSlash = strrchr(g_ModDir, '\\');
        if (lastSlash) *(lastSlash + 1) = '\0';

        char origPath[MAX_PATH];
        snprintf(origPath, sizeof(origPath), "%sbass_orig.dll", g_ModDir);
        HMODULE hOrig = LoadLibraryA(origPath);
        if (!hOrig) {
            MessageBoxA(nullptr, "Could not load bass_orig.dll! Make sure the original bass.dll was renamed to bass_orig.dll.", "PeggleOnline Mod", MB_ICONERROR);
            return FALSE;
        }
        for (int i = 0; i < 95; i++) {
            g_BassProcs[i] = GetProcAddress(hOrig, kBassExportNames[i]);
        }

        InstallHooksIfInnerGame();
    }
    return TRUE;
}
