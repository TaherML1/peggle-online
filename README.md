# Peggle Nights Online Duel

A WebSocket room-code relay, custom leaderboard server, and 32-bit C++ proxy DLL mod (`bass.dll`) + 1-click Win32 launcher (`PlayPeggleOnline.exe`) for playing **Peggle Nights** **1v1 Duel** and **2v2 Team Duel** online with friends.

## Features
- **1v1 & 2v2 Team Duel:** 4-player turn rotation (`P1 → P2 → P3 → P4`) across Team A (P1 & P3) and Team B (P2 & P4) with live turn locking & title-bar HUD.
- **Configurable Balls per Player:** Choose **3**, **4 (Recommended)**, **5**, or **6** balls per player in the launcher.
- **Custom Leaderboards:** Create a custom leaderboard code right from the launcher or web dashboard (`https://your-service.onrender.com/leaderboard`) and automatically record 1v1 & 2v2 wins, scores, and match history!

## 1-Click Deploy Relay to Render (Free Tier)

[![Deploy to Render](https://render.com/images/deploy-to-render-button.svg)](https://render.com/deploy?repo=https://github.com/TaherML1/peggle-online)

Or manually in Render:
1. **New +** -> **Web Service** -> connect `TaherML1/peggle-online`.
2. **Runtime:** `Node` | **Build Command:** *(empty)* | **Start Command:** `node server.mjs` | **Plan:** `Free`.
3. Copy your deployed Render URL (e.g. `https://peggle-online-relay.onrender.com`).

## How to Play

1. Paste your deployed Render URL into `PlayPeggleOnline.exe` (or `relay_config.ini`).
2. Enter your display name, pick **1v1** or **2v2**, pick your **Team Slot** (`Player 1 (Host)` sets Balls/Player), and optionally enter/create a **Custom Leaderboard Code**.
3. Share the 4-letter **Room Code** with your friends and click **Play Online Duel!**.
4. Once the title bar shows **`ALL READY!`**, everyone enters **Duel** -> picks the **same level** -> clicks **PLAY A FRIEND** -> clicks **PLAY NOW!**.

## Building the Mod from Source (`mod/`)

Requires Visual Studio with 32-bit (`x86`) MSVC C++ build tools and Windows 10/11 SDK (`winhttp.h`):
```bat
cd mod
build.bat
```
