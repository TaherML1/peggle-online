# Peggle Nights Online Duel

A WebSocket room-code relay server and 32-bit C++ proxy DLL mod (`bass.dll`) + 1-click Win32 launcher (`PlayPeggleOnline.exe`) for playing **Peggle Nights** 2-player **Duel Mode** online with a friend.

## 1-Click Deploy Relay to Render (Free Tier)

[![Deploy to Render](https://render.com/images/deploy-to-render-button.svg)](https://render.com/deploy?repo=https://github.com/TaherML1/peggle-online)

Or manually in Render:
1. **New +** -> **Web Service** -> connect `TaherML1/peggle-online`.
2. **Runtime:** `Node` | **Build Command:** *(empty)* | **Start Command:** `node server.mjs` | **Plan:** `Free`.
3. Copy your deployed Render URL (e.g. `https://peggle-online-relay.onrender.com`).

## How to Play

1. Paste your deployed Render URL into `PlayPeggleOnline.exe` (or `relay_config.ini`).
2. **Player 1 (Host):** Choose **Host a Match**, copy the 4-letter Room Code, and click **Play Online Duel!**.
3. **Player 2 (Friend):** Choose **Join a Match**, enter the same Render URL and Room Code, and click **Play Online Duel!**.
4. Once the title bar shows **`FRIEND JOINED!`**, both players enter **Duel** -> pick the **same level** -> click **PLAY A FRIEND** -> click **PLAY NOW!**.

## Building the Mod from Source (`mod/`)

Requires Visual Studio with 32-bit (`x86`) MSVC C++ build tools and Windows 10/11 SDK (`winhttp.h`):
```bat
cd mod
build.bat
```
