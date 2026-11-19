import http from "node:http";
import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";

const PORT = Number(process.env.PORT || 10000);
const WS_MAGIC = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
const MAX_MSG_BYTES = 4 * 1024 * 1024;
const PING_INTERVAL_MS = 20_000;
const LEADERBOARD_FILE = path.resolve(process.env.LEADERBOARD_FILE || "./leaderboards.json");

/** @type {Map<string, { roomCode: string, maxPlayers: number, leaderboardCode: string, slots: (WsConn | null)[], names: string[], recordedMatchIds: Set<string> }>} */
const rooms = new Map();

/** @type {Map<string, { code: string, name: string, createdAt: string, updatedAt: string, players: Record<string, any>, matches: any[], matchIds: string[] }>} */
const leaderboards = new Map();

function loadLeaderboardsFromDisk() {
  try {
    if (!fs.existsSync(LEADERBOARD_FILE)) return;
    const raw = JSON.parse(fs.readFileSync(LEADERBOARD_FILE, "utf8"));
    if (!raw || typeof raw !== "object") return;
    for (const [code, lb] of Object.entries(raw)) {
      if (lb && typeof lb === "object" && typeof lb.code === "string") {
        leaderboards.set(code, {
          code: lb.code,
          name: String(lb.name || lb.code),
          createdAt: String(lb.createdAt || new Date().toISOString()),
          updatedAt: String(lb.updatedAt || lb.createdAt || new Date().toISOString()),
          players: lb.players && typeof lb.players === "object" ? lb.players : {},
          matches: Array.isArray(lb.matches) ? lb.matches : [],
          matchIds: Array.isArray(lb.matchIds) ? lb.matchIds : [],
        });
      }
    }
    console.log(`[Leaderboard] Loaded ${leaderboards.size} leaderboard(s) from ${LEADERBOARD_FILE}`);
  } catch (e) {
    console.warn("[Leaderboard] Could not load disk file:", e?.message || e);
  }
}

function saveLeaderboardsToDisk() {
  try {
    const obj = {};
    for (const [code, lb] of leaderboards.entries()) {
      obj[code] = lb;
    }
    const dir = path.dirname(LEADERBOARD_FILE);
    fs.mkdirSync(dir, { recursive: true });
    const tmp = `${LEADERBOARD_FILE}.tmp`;
    fs.writeFileSync(tmp, JSON.stringify(obj, null, 2), "utf8");
    fs.renameSync(tmp, LEADERBOARD_FILE);
  } catch (e) {
    console.warn("[Leaderboard] Could not save to disk:", e?.message || e);
  }
}

loadLeaderboardsFromDisk();

function sanitizeName(raw, fallback) {
  const s = String(raw || "")
    .replace(/[\x00-\x1f\x7f]/g, "")
    .trim()
    .slice(0, 24);
  return s || fallback;
}

function sanitizeLbCode(raw) {
  const s = String(raw || "")
    .trim()
    .toUpperCase()
    .replace(/[^A-Z0-9-]/g, "")
    .slice(0, 16);
  return s.length >= 2 ? s : "";
}

function generateLbCodeFromName(name) {
  const alpha = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
  const prefix = String(name || "")
    .toUpperCase()
    .replace(/[^A-Z0-9]/g, "")
    .slice(0, 5);
  for (let attempt = 0; attempt < 20; attempt++) {
    let suffix = "";
    for (let i = 0; i < (prefix.length >= 3 ? 3 : 5); i++) {
      suffix += alpha[Math.floor(Math.random() * alpha.length)];
    }
    const candidate = prefix.length >= 3 ? `${prefix}-${suffix}` : suffix;
    if (!leaderboards.has(candidate)) return candidate;
  }
  return `LB${Date.now().toString(36).toUpperCase().slice(-6)}`;
}

function getOrCreateLeaderboard(code, displayName) {
  let lb = leaderboards.get(code);
  if (!lb) {
    const now = new Date().toISOString();
    lb = {
      code,
      name: sanitizeName(displayName, code).slice(0, 48),
      createdAt: now,
      updatedAt: now,
      players: {},
      matches: [],
      matchIds: [],
    };
    leaderboards.set(code, lb);
    saveLeaderboardsToDisk();
  }
  return lb;
}

function recordMatchToLeaderboard(code, payload) {
  const cleanCode = sanitizeLbCode(code);
  if (!cleanCode) return null;
  const lb = getOrCreateLeaderboard(cleanCode, payload?.leaderboardName || cleanCode);

  const matchId = payload?.matchId ? String(payload.matchId).slice(0, 64) : "";
  if (matchId && lb.matchIds.includes(matchId)) {
    return lb.matches.find((m) => m.matchId === matchId) || null;
  }

  const mode = payload?.mode === "2v2" ? "2v2" : "1v1";
  const ballsPerPlayer = Number(payload?.ballsPerPlayer) || 0;
  const scoreA = Math.max(0, Math.trunc(Number(payload?.scoreA) || 0));
  const scoreB = Math.max(0, Math.trunc(Number(payload?.scoreB) || 0));
  const rawTeamA = Array.isArray(payload?.teamA) ? payload.teamA : [];
  const rawTeamB = Array.isArray(payload?.teamB) ? payload.teamB : [];
  const teamA = rawTeamA.map((n, idx) => sanitizeName(n, idx === 0 ? "Player 1" : "Player 3"));
  const teamB = rawTeamB.map((n, idx) => sanitizeName(n, idx === 0 ? "Player 2" : "Player 4"));
  if (teamA.length === 0) teamA.push("Player 1");
  if (teamB.length === 0) teamB.push("Player 2");

  const winner = scoreA > scoreB ? "A" : scoreB > scoreA ? "B" : "DRAW";
  const now = new Date().toISOString();

  const updateSide = (names, myScore, sideTag) => {
    for (const pName of names) {
      const key = pName.toLowerCase();
      if (!lb.players[key]) {
        lb.players[key] = {
          name: pName,
          wins: 0,
          losses: 0,
          draws: 0,
          matches: 0,
          totalScore: 0,
          bestScore: 0,
          wins1v1: 0,
          wins2v2: 0,
        };
      }
      const p = lb.players[key];
      p.name = pName;
      p.matches += 1;
      p.totalScore += myScore;
      if (myScore > p.bestScore) p.bestScore = myScore;
      if (winner === "DRAW") {
        p.draws += 1;
      } else if (winner === sideTag) {
        p.wins += 1;
        if (mode === "2v2") p.wins2v2 += 1;
        else p.wins1v1 += 1;
      } else {
        p.losses += 1;
      }
    }
  };

  updateSide(teamA, scoreA, "A");
  updateSide(teamB, scoreB, "B");

  const matchRecord = {
    matchId: matchId || `m_${Date.now()}_${Math.random().toString(36).slice(2, 7)}`,
    mode,
    ballsPerPlayer,
    teamA,
    teamB,
    scoreA,
    scoreB,
    winner,
    playedAt: now,
  };

  lb.matches.unshift(matchRecord);
  if (lb.matches.length > 100) lb.matches.length = 100;
  if (matchId) {
    lb.matchIds.unshift(matchId);
    if (lb.matchIds.length > 250) lb.matchIds.length = 250;
  }
  lb.updatedAt = now;
  saveLeaderboardsToDisk();
  console.log(`[Leaderboard ${cleanCode}] Recorded ${mode} match: ${teamA.join("+")} (${scoreA}) vs ${teamB.join("+")} (${scoreB})`);
  return matchRecord;
}

function getSortedStandings(lb) {
  return Object.values(lb.players)
    .map((p) => ({
      ...p,
      winPct: p.matches > 0 ? Math.round((p.wins / p.matches) * 100) : 0,
    }))
    .sort((a, b) => {
      if (b.wins !== a.wins) return b.wins - a.wins;
      if (b.totalScore !== a.totalScore) return b.totalScore - a.totalScore;
      return b.bestScore - a.bestScore;
    });
}

function recordMatchFromRoom(room, payload) {
  const lbCode = sanitizeLbCode(payload?.leaderboard || room.leaderboardCode);
  if (!lbCode) return null;
  const matchId = payload?.matchId ? String(payload.matchId).slice(0, 64) : "";
  if (matchId && room.recordedMatchIds.has(matchId)) return null;
  if (matchId) room.recordedMatchIds.add(matchId);

  const is2v2 = room.maxPlayers === 4 || payload?.mode === "2v2";
  const teamA = is2v2 ? [room.names[0], room.names[2]] : [room.names[0]];
  const teamB = is2v2 ? [room.names[1], room.names[3]] : [room.names[1]];
  return recordMatchToLeaderboard(lbCode, {
    ...payload,
    mode: is2v2 ? "2v2" : "1v1",
    teamA,
    teamB,
  });
}

// ============================================================================
// WebSocket Framing & Multi-Player Room Relay (2 or 4 players)
// ============================================================================
function encodeFrame(opcode, payload) {
  const data = Buffer.isBuffer(payload) ? payload : Buffer.from(payload);
  const len = data.length;
  let hdr;
  if (len < 126) {
    hdr = Buffer.allocUnsafe(2);
    hdr[0] = 0x80 | (opcode & 0x0f);
    hdr[1] = len;
  } else if (len < 65536) {
    hdr = Buffer.allocUnsafe(4);
    hdr[0] = 0x80 | (opcode & 0x0f);
    hdr[1] = 126;
    hdr.writeUInt16BE(len, 2);
  } else {
    hdr = Buffer.allocUnsafe(10);
    hdr[0] = 0x80 | (opcode & 0x0f);
    hdr[1] = 127;
    hdr.writeBigUInt64BE(BigInt(len), 2);
  }
  return len > 0 ? Buffer.concat([hdr, data]) : hdr;
}

function encodeCloseFrame(code) {
  const buf = Buffer.allocUnsafe(2);
  buf.writeUInt16BE(code, 0);
  return encodeFrame(0x8, buf);
}

class WsConn {
  constructor(socket, roomCode, slot) {
    this.socket = socket;
    this.roomCode = roomCode;
    this.slot = slot;
    this.closed = false;
    this.rxBuf = Buffer.alloc(0);
    this.fragChunks = [];
    this.fragBytes = 0;
  }

  sendBinary(buf) {
    if (this.closed || this.socket.destroyed) return;
    try {
      this.socket.write(encodeFrame(0x2, buf));
    } catch {
      this.destroy();
    }
  }

  sendPing() {
    if (this.closed || this.socket.destroyed) return;
    try {
      this.socket.write(encodeFrame(0x9, Buffer.alloc(0)));
    } catch {
      this.destroy();
    }
  }

  closeWithCode(code) {
    if (this.closed) return;
    this.closed = true;
    try {
      if (!this.socket.destroyed) {
        this.socket.end(encodeCloseFrame(code));
        setTimeout(() => {
          if (!this.socket.destroyed) this.socket.destroy();
        }, 250).unref();
      }
    } catch {
      this.socket.destroy();
    }
  }

  destroy() {
    if (this.closed) return;
    this.closed = true;
    if (!this.socket.destroyed) {
      this.socket.destroy();
    }
  }
}

function getOrCreateRoom(roomCode, maxPlayers, leaderboardCode) {
  let room = rooms.get(roomCode);
  if (!room) {
    room = {
      roomCode,
      maxPlayers,
      leaderboardCode: leaderboardCode || "",
      slots: [null, null, null, null],
      names: ["Player 1", "Player 2", "Player 3", "Player 4"],
      recordedMatchIds: new Set(),
    };
    rooms.set(roomCode, room);
  }
  return room;
}

function sendRoomStatus(room) {
  let count = 0;
  let mask = 0;
  for (let i = 0; i < room.maxPlayers; i++) {
    if (room.slots[i]) {
      count++;
      mask |= 1 << i;
    }
  }
  const pgst = Buffer.from([0x50, 0x47, 0x53, 0x54, count, room.maxPlayers, mask]);
  for (let i = 0; i < room.slots.length; i++) {
    if (room.slots[i]) room.slots[i].sendBinary(pgst);
  }
  if (count === room.maxPlayers) {
    console.log(`[Relay] Room ${room.roomCode}: ${count}/${room.maxPlayers} ready -> sending PGOK`);
    const pgok = Buffer.from("PGOK");
    for (let i = 0; i < room.maxPlayers; i++) {
      if (room.slots[i]) room.slots[i].sendBinary(pgok);
    }
  }
}

function detachConnection(conn) {
  conn.closed = true;
  const room = rooms.get(conn.roomCode);
  if (!room) return;
  if (room.slots[conn.slot] !== conn) return;

  room.slots[conn.slot] = null;
  let remaining = 0;
  let mask = 0;
  for (let i = 0; i < room.maxPlayers; i++) {
    if (room.slots[i]) {
      remaining++;
      mask |= 1 << i;
    }
  }
  const pgst = Buffer.from([0x50, 0x47, 0x53, 0x54, remaining, room.maxPlayers, mask]);
  const pgby = Buffer.from("PGBY");
  for (let i = 0; i < room.slots.length; i++) {
    if (room.slots[i]) {
      room.slots[i].sendBinary(pgst);
      room.slots[i].sendBinary(pgby);
    }
  }
  if (!room.slots.some(Boolean)) {
    rooms.delete(conn.roomCode);
  }
  console.log(`[Relay] Room ${conn.roomCode}: slot ${conn.slot} disconnected (${remaining}/${room.maxPlayers} left)`);
}

function handleMessage(conn, msgBuf) {
  const room = rooms.get(conn.roomCode);
  if (!room) return;

  // Packet type 5 = PKT_MATCH_RESULT ([uint8 type=5][uint32 len][UTF-8 JSON])
  if (msgBuf.length >= 5 && msgBuf[0] === 5) {
    const plen = msgBuf.readUInt32LE(1);
    if (5 + plen === msgBuf.length) {
      try {
        const json = JSON.parse(msgBuf.subarray(5).toString("utf8"));
        recordMatchFromRoom(room, json);
      } catch (e) {
        console.warn("[Leaderboard] Invalid match JSON:", e?.message || e);
      }
      return;
    }
  }

  for (let i = 0; i < room.slots.length; i++) {
    if (i !== conn.slot && room.slots[i]) {
      room.slots[i].sendBinary(msgBuf);
    }
  }
}

function processRxBuffer(conn) {
  while (!conn.closed && conn.rxBuf.length >= 2) {
    const b0 = conn.rxBuf[0];
    const b1 = conn.rxBuf[1];
    const fin = (b0 & 0x80) !== 0;
    const opcode = b0 & 0x0f;
    const masked = (b1 & 0x80) !== 0;
    let payloadLen = b1 & 0x7f;
    let offset = 2;

    if (payloadLen === 126) {
      if (conn.rxBuf.length < offset + 2) return;
      payloadLen = conn.rxBuf.readUInt16BE(offset);
      offset += 2;
    } else if (payloadLen === 127) {
      if (conn.rxBuf.length < offset + 8) return;
      const bigLen = conn.rxBuf.readBigUInt64BE(offset);
      if (bigLen > BigInt(MAX_MSG_BYTES)) {
        conn.closeWithCode(1009);
        detachConnection(conn);
        return;
      }
      payloadLen = Number(bigLen);
      offset += 8;
    }

    if (payloadLen > MAX_MSG_BYTES) {
      conn.closeWithCode(1009);
      detachConnection(conn);
      return;
    }

    let maskKey = null;
    if (masked) {
      if (conn.rxBuf.length < offset + 4) return;
      maskKey = conn.rxBuf.subarray(offset, offset + 4);
      offset += 4;
    }

    if (conn.rxBuf.length < offset + payloadLen) return;

    const payload = Buffer.allocUnsafe(payloadLen);
    conn.rxBuf.copy(payload, 0, offset, offset + payloadLen);
    conn.rxBuf = conn.rxBuf.subarray(offset + payloadLen);

    if (masked && maskKey) {
      for (let i = 0; i < payloadLen; i++) {
        payload[i] ^= maskKey[i & 3];
      }
    }

    if (opcode === 0x8) {
      conn.closeWithCode(1000);
      detachConnection(conn);
      return;
    }
    if (opcode === 0x9) {
      if (!conn.closed && !conn.socket.destroyed) {
        try {
          conn.socket.write(encodeFrame(0xA, payload));
        } catch {
          conn.destroy();
        }
      }
      continue;
    }
    if (opcode === 0xA) {
      continue;
    }
    if (opcode === 0x1 || opcode === 0x2 || opcode === 0x0) {
      if (opcode !== 0x0) {
        conn.fragChunks = [payload];
        conn.fragBytes = payload.length;
      } else {
        conn.fragChunks.push(payload);
        conn.fragBytes += payload.length;
      }
      if (conn.fragBytes > MAX_MSG_BYTES) {
        conn.fragChunks = [];
        conn.fragBytes = 0;
        conn.closeWithCode(1009);
        detachConnection(conn);
        return;
      }
      if (fin) {
        const fullMsg =
          conn.fragChunks.length === 1
            ? conn.fragChunks[0]
            : Buffer.concat(conn.fragChunks, conn.fragBytes);
        conn.fragChunks = [];
        conn.fragBytes = 0;
        handleMessage(conn, fullMsg);
      }
    }
  }
}

// ============================================================================
// HTTP Server (Health, Leaderboard REST API, & Web Dashboard)
// ============================================================================
function sendJson(res, status, data) {
  res.writeHead(status, {
    "Content-Type": "application/json; charset=utf-8",
    "Cache-Control": "no-store",
  });
  res.end(JSON.stringify(data));
}

function readJsonBody(req, maxBytes = 16384) {
  return new Promise((resolve, reject) => {
    const chunks = [];
    let total = 0;
    req.on("data", (c) => {
      total += c.length;
      if (total > maxBytes) {
        reject(new Error("Body too large"));
        req.destroy();
        return;
      }
      chunks.push(c);
    });
    req.on("end", () => {
      try {
        const str = Buffer.concat(chunks).toString("utf8").trim();
        resolve(str ? JSON.parse(str) : {});
      } catch {
        reject(new Error("Invalid JSON"));
      }
    });
    req.on("error", reject);
  });
}

function renderLeaderboardHtml(initialCode = "") {
  const safeInit = JSON.stringify(initialCode || "");
  return `<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width,initial-scale=1" />
  <title>Peggle Nights Online — Custom Leaderboards</title>
  <style>
    :root {
      --bg1: #0b0a1f;
      --bg2: #17143d;
      --card: rgba(25, 22, 62, 0.88);
      --border: rgba(255, 255, 255, 0.12);
      --text: #f3f5ff;
      --muted: #a6add4;
      --orange: #ff8a1f;
      --gold: #ffd147;
      --green: #4ade80;
      --blue: #58b4ff;
    }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      min-height: 100vh;
      font-family: "Segoe UI", system-ui, -apple-system, sans-serif;
      color: var(--text);
      background: radial-gradient(circle at top right, #2e1d6b 0%, var(--bg2) 45%, var(--bg1) 100%);
      padding: 24px 16px 48px;
    }
    .wrap { max-width: 960px; margin: 0 auto; }
    header {
      display: flex; flex-wrap: wrap; align-items: center; justify-content: space-between;
      gap: 12px; margin-bottom: 22px;
    }
    .brand { display: flex; align-items: center; gap: 12px; }
    .peg {
      width: 40px; height: 40px; border-radius: 50%;
      background: radial-gradient(circle at 30% 30%, #ffd580, #ff6a00 70%, #992e00);
      box-shadow: 0 0 18px rgba(255, 138, 31, 0.6);
      flex: none;
    }
    h1 { margin: 0; font-size: 1.55rem; letter-spacing: 0.3px; }
    .sub { color: var(--muted); font-size: 0.92rem; margin-top: 2px; }
    .card {
      background: var(--card);
      border: 1px solid var(--border);
      border-radius: 16px;
      padding: 20px;
      margin-bottom: 18px;
      box-shadow: 0 12px 32px rgba(0,0,0,0.35);
    }
    .grid2 { display: grid; grid-template-columns: repeat(auto-fit, minmax(280px, 1fr)); gap: 16px; }
    label { display: block; font-size: 0.85rem; color: var(--muted); margin-bottom: 6px; font-weight: 600; }
    input {
      width: 100%; padding: 10px 12px; border-radius: 10px;
      border: 1px solid var(--border); background: rgba(9, 8, 26, 0.7);
      color: var(--text); font-size: 0.96rem; margin-bottom: 10px;
    }
    input:focus { outline: 2px solid var(--orange); border-color: transparent; }
    .row { display: flex; gap: 8px; flex-wrap: wrap; align-items: center; }
    button, .btn {
      cursor: pointer; border: 0; border-radius: 10px; padding: 10px 16px;
      font-weight: 700; font-size: 0.92rem; text-decoration: none; display: inline-flex; align-items: center; gap: 6px;
      background: linear-gradient(135deg, #ff9922, #ff5b1f); color: #160c00;
      transition: transform 0.1s ease, filter 0.15s ease;
    }
    button:hover, .btn:hover { filter: brightness(1.08); transform: translateY(-1px); }
    button.sec, .btn.sec {
      background: rgba(255,255,255,0.1); color: var(--text); border: 1px solid var(--border);
    }
    .badge {
      display: inline-block; padding: 4px 10px; border-radius: 999px;
      background: rgba(255, 138, 31, 0.18); border: 1px solid rgba(255, 138, 31, 0.45);
      color: var(--gold); font-family: Consolas, monospace; font-weight: 700; font-size: 0.9rem;
    }
    table { width: 100%; border-collapse: collapse; margin-top: 10px; }
    th, td { text-align: left; padding: 10px 8px; border-bottom: 1px solid rgba(255,255,255,0.08); }
    th { color: var(--muted); font-size: 0.82rem; text-transform: uppercase; letter-spacing: 0.5px; }
    tr:hover td { background: rgba(255,255,255,0.03); }
    .rank1 { color: var(--gold); font-weight: 800; }
    .match {
      display: flex; flex-wrap: wrap; justify-content: space-between; align-items: center; gap: 10px;
      padding: 12px 14px; border-radius: 12px; background: rgba(10, 9, 30, 0.55);
      border: 1px solid rgba(255,255,255,0.07); margin-bottom: 8px;
    }
    .win { color: var(--green); font-weight: 700; }
    .tag { font-size: 0.78rem; padding: 2px 8px; border-radius: 6px; background: rgba(88,180,255,0.16); color: var(--blue); font-weight: 700; }
    .lb-pill {
      display: flex; justify-content: space-between; align-items: center;
      padding: 10px 14px; border-radius: 10px; background: rgba(10,9,30,0.5);
      border: 1px solid rgba(255,255,255,0.08); margin-bottom: 8px; text-decoration: none; color: var(--text);
    }
    .lb-pill:hover { border-color: var(--orange); }
    .msg { color: var(--gold); font-size: 0.9rem; margin-top: 8px; min-height: 20px; }
    footer { text-align: center; color: var(--muted); font-size: 0.82rem; margin-top: 24px; }
  </style>
</head>
<body>
  <div class="wrap">
    <header>
      <div class="brand">
        <div class="peg"></div>
        <div>
          <h1>Peggle Nights Online — Leaderboards</h1>
          <div class="sub">1v1 &amp; 2v2 Team Duel Standings</div>
        </div>
      </div>
      <div id="navActions" class="row"></div>
    </header>
    <main id="app"></main>
    <footer>Note: Render Free Tier sleeps after ~15 min of inactivity and may reset local JSON storage on restart.</footer>
  </div>
  <script>
    const INITIAL_CODE = ${safeInit};
    const app = document.getElementById("app");
    const navActions = document.getElementById("navActions");

    function esc(s) {
      return String(s ?? "").replace(/[&<>"']/g, (c) => ({
        "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;"
      })[c]);
    }
    function fmtNum(n) { return Number(n || 0).toLocaleString(); }

    async function renderHome() {
      navActions.innerHTML = "";
      app.innerHTML = \`
        <div class="grid2">
          <div class="card">
            <h2 style="margin-top:0">Create Your Leaderboard</h2>
            <label>Leaderboard Name</label>
            <input id="lbName" placeholder="e.g. Taher &amp; Friends Club" maxlength="40" />
            <label>Custom Code (optional, 2–16 letters/numbers)</label>
            <input id="lbCode" placeholder="Leave blank to auto-generate (e.g. TAHER)" maxlength="16" style="text-transform:uppercase" />
            <button id="btnCreate">Create Leaderboard</button>
            <div id="createMsg" class="msg"></div>
          </div>
          <div class="card">
            <h2 style="margin-top:0">Open Existing Leaderboard</h2>
            <label>Leaderboard Code</label>
            <input id="openCode" placeholder="Enter code (e.g. TAHER)" maxlength="16" style="text-transform:uppercase" />
            <button id="btnOpen" class="sec">Open Leaderboard</button>
          </div>
        </div>
        <div class="card">
          <h2 style="margin-top:0">Active Leaderboards</h2>
          <div id="lbList"><span class="sub">Loading...</span></div>
        </div>
      \`;

      document.getElementById("btnCreate").onclick = async () => {
        const name = document.getElementById("lbName").value.trim();
        const code = document.getElementById("lbCode").value.trim().toUpperCase();
        const msg = document.getElementById("createMsg");
        if (!name) { msg.textContent = "Please enter a leaderboard name."; return; }
        const res = await fetch("/api/leaderboards", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ name, code })
        });
        const data = await res.json();
        if (!res.ok) { msg.textContent = data.error || "Could not create leaderboard."; return; }
        location.href = "/leaderboard/" + encodeURIComponent(data.code);
      };

      document.getElementById("btnOpen").onclick = () => {
        const c = document.getElementById("openCode").value.trim().toUpperCase().replace(/[^A-Z0-9-]/g, "");
        if (c.length >= 2) location.href = "/leaderboard/" + encodeURIComponent(c);
      };

      const res = await fetch("/api/leaderboards");
      const list = await res.json();
      const box = document.getElementById("lbList");
      if (!list.length) {
        box.innerHTML = '<div class="sub">No leaderboards created yet. Create the first one above!</div>';
        return;
      }
      box.innerHTML = list.map((lb) => \`
        <a class="lb-pill" href="/leaderboard/\${encodeURIComponent(lb.code)}">
          <div>
            <strong>\${esc(lb.name)}</strong>
            <span class="badge" style="margin-left:8px">\${esc(lb.code)}</span>
          </div>
          <div class="sub">\${lb.playerCount} players &bull; \${lb.matchCount} matches</div>
        </a>
      \`).join("");
    }

    async function renderBoard(code) {
      navActions.innerHTML = '<a class="btn sec" href="/leaderboard">All Leaderboards</a>';
      async function load() {
        const res = await fetch("/api/leaderboards/" + encodeURIComponent(code));
        if (!res.ok) {
          app.innerHTML = \`
            <div class="card">
              <h2>Leaderboard Not Found: <span class="badge">\${esc(code)}</span></h2>
              <p class="sub">No matches or leaderboard exist for this code yet (or Render restarted).</p>
              <button id="btnQuickCreate">Create "\${esc(code)}" Now</button>
            </div>
          \`;
          document.getElementById("btnQuickCreate").onclick = async () => {
            await fetch("/api/leaderboards", {
              method: "POST",
              headers: { "Content-Type": "application/json" },
              body: JSON.stringify({ name: code, code, getOrCreate: true })
            });
            load();
          };
          return;
        }
        const lb = await res.json();
        const standingsRows = lb.standings.length
          ? lb.standings.map((p, i) => \`
              <tr>
                <td class="\${i === 0 ? "rank1" : ""}">#\${i + 1}</td>
                <td><strong>\${esc(p.name)}</strong></td>
                <td>\${p.wins}-\${p.losses}-\${p.draws}</td>
                <td>\${p.winPct}%</td>
                <td>\${fmtNum(p.bestScore)}</td>
                <td>\${fmtNum(p.totalScore)}</td>
                <td>\${p.matches}</td>
              </tr>
            \`).join("")
          : '<tr><td colspan="7" class="sub">No matches played yet! Enter this Leaderboard Code in PlayPeggleOnline.exe and finish a Duel match.</td></tr>';

        const matchCards = lb.matches.length
          ? lb.matches.slice(0, 25).map((m) => \`
              <div class="match">
                <div>
                  <span class="tag">\${esc(m.mode)}\${m.ballsPerPlayer ? " &bull; " + esc(m.ballsPerPlayer) + " balls/p" : ""}</span>
                  <span style="margin-left:8px" class="\${m.winner === "A" ? "win" : ""}">\${esc(m.teamA.join(" & "))} (\${fmtNum(m.scoreA)})</span>
                  <span class="sub"> vs </span>
                  <span class="\${m.winner === "B" ? "win" : ""}">\${esc(m.teamB.join(" & "))} (\${fmtNum(m.scoreB)})</span>
                </div>
                <div class="sub">\${esc(new Date(m.playedAt).toLocaleString())}</div>
              </div>
            \`).join("")
          : '<div class="sub">No matches recorded yet.</div>';

        app.innerHTML = \`
          <div class="card">
            <div class="row" style="justify-content:space-between">
              <div>
                <h2 style="margin:0 0 6px 0">\${esc(lb.name)} <span class="badge">\${esc(lb.code)}</span></h2>
                <div class="sub">Put code <strong>\${esc(lb.code)}</strong> in <em>PlayPeggleOnline.exe</em> &rarr; Custom Leaderboard to record your games here automatically!</div>
              </div>
              <div class="row">
                <button id="btnCopyCode" class="sec">Copy Code</button>
                <button id="btnCopyUrl">Copy Invite Link</button>
              </div>
            </div>
            <div id="copyStatus" class="msg"></div>
          </div>
          <div class="card">
            <h3 style="margin-top:0">Standings</h3>
            <table>
              <thead>
                <tr><th>Rank</th><th>Player</th><th>W-L-D</th><th>Win %</th><th>Best Score</th><th>Total Score</th><th>Games</th></tr>
              </thead>
              <tbody>\${standingsRows}</tbody>
            </table>
          </div>
          <div class="card">
            <h3 style="margin-top:0">Recent Matches</h3>
            \${matchCards}
          </div>
        \`;
        document.getElementById("btnCopyCode").onclick = async () => {
          await navigator.clipboard.writeText(lb.code);
          document.getElementById("copyStatus").textContent = "Copied leaderboard code " + lb.code + "!";
        };
        document.getElementById("btnCopyUrl").onclick = async () => {
          await navigator.clipboard.writeText(location.href);
          document.getElementById("copyStatus").textContent = "Copied leaderboard link!";
        };
      }
      await load();
      setInterval(load, 15000);
    }

    if (INITIAL_CODE) renderBoard(INITIAL_CODE);
    else renderHome();
  </script>
</body>
</html>`;
}

const server = http.createServer(async (req, res) => {
  const url = new URL(req.url || "/", `http://${req.headers.host || "localhost"}`);
  const p = url.pathname;

  if (req.method === "GET" && p === "/health") {
    return sendJson(res, 200, {
      ok: true,
      service: "peggle-online-relay",
      version: 2,
      activeRooms: rooms.size,
      leaderboards: leaderboards.size,
    });
  }

  if (req.method === "GET" && p === "/api/leaderboards") {
    const list = Array.from(leaderboards.values())
      .map((lb) => ({
        code: lb.code,
        name: lb.name,
        playerCount: Object.keys(lb.players).length,
        matchCount: lb.matches.length,
        updatedAt: lb.updatedAt,
      }))
      .sort((a, b) => b.updatedAt.localeCompare(a.updatedAt));
    return sendJson(res, 200, list);
  }

  if (req.method === "POST" && p === "/api/leaderboards") {
    try {
      const body = await readJsonBody(req);
      const name = sanitizeName(body?.name, "");
      if (!name) return sendJson(res, 400, { ok: false, error: "Leaderboard name is required." });

      let code = body?.code ? sanitizeLbCode(body.code) : "";
      if (body?.code && !code) {
        return sendJson(res, 400, { ok: false, error: "Code must be 2–16 letters/numbers." });
      }
      if (!code) code = generateLbCodeFromName(name);

      if (leaderboards.has(code) && !body?.getOrCreate) {
        return sendJson(res, 409, { ok: false, error: `Leaderboard code "${code}" already exists.` });
      }
      const lb = getOrCreateLeaderboard(code, name);
      return sendJson(res, 201, {
        ok: true,
        code: lb.code,
        name: lb.name,
        url: `/leaderboard/${lb.code}`,
      });
    } catch (e) {
      return sendJson(res, 400, { ok: false, error: e?.message || "Bad request" });
    }
  }

  const lbApiMatch = p.match(/^\/api\/leaderboards\/([A-Za-z0-9-]{2,16})(\/matches)?$/);
  if (lbApiMatch) {
    const code = sanitizeLbCode(lbApiMatch[1]);
    if (!code) return sendJson(res, 400, { ok: false, error: "Invalid code" });
    const isMatchesPost = Boolean(lbApiMatch[2]);

    if (req.method === "GET" && !isMatchesPost) {
      const lb = leaderboards.get(code);
      if (!lb) return sendJson(res, 404, { ok: false, error: "Leaderboard not found" });
      return sendJson(res, 200, {
        ok: true,
        code: lb.code,
        name: lb.name,
        createdAt: lb.createdAt,
        updatedAt: lb.updatedAt,
        standings: getSortedStandings(lb),
        matches: lb.matches,
      });
    }

    if (req.method === "POST" && isMatchesPost) {
      try {
        const body = await readJsonBody(req);
        const match = recordMatchToLeaderboard(code, body);
        return sendJson(res, 200, { ok: true, code, match });
      } catch (e) {
        return sendJson(res, 400, { ok: false, error: e?.message || "Bad request" });
      }
    }
  }

  if (req.method === "GET" && (p === "/" || p === "/leaderboard" || p === "/leaderboard/")) {
    res.writeHead(200, { "Content-Type": "text/html; charset=utf-8", "Cache-Control": "no-store" });
    res.end(renderLeaderboardHtml(""));
    return;
  }

  const lbPageMatch = p.match(/^\/leaderboard\/([A-Za-z0-9-]{2,16})\/?$/);
  if (req.method === "GET" && lbPageMatch) {
    const code = sanitizeLbCode(lbPageMatch[1]);
    res.writeHead(200, { "Content-Type": "text/html; charset=utf-8", "Cache-Control": "no-store" });
    res.end(renderLeaderboardHtml(code));
    return;
  }

  res.writeHead(404, { "Content-Type": "text/plain" });
  res.end("Not found");
});

server.on("upgrade", (req, socket, head) => {
  let url;
  try {
    url = new URL(req.url || "/", `http://${req.headers.host || "localhost"}`);
  } catch {
    socket.end("HTTP/1.1 400 Bad Request\r\n\r\n");
    return;
  }

  const roomCode = (url.searchParams.get("room") || "").trim().toUpperCase();
  const wsKey = req.headers["sec-websocket-key"];

  let maxPlayers = Number(url.searchParams.get("max") || "2");
  if (maxPlayers !== 2 && maxPlayers !== 4) maxPlayers = 0;

  let slot = -1;
  const slotParam = url.searchParams.get("slot");
  if (slotParam !== null && slotParam !== "") {
    const parsed = Number(slotParam);
    if (Number.isInteger(parsed) && parsed >= 0 && parsed < 4) slot = parsed;
  } else {
    const role = (url.searchParams.get("role") || "").trim().toLowerCase();
    if (role === "host") { slot = 0; maxPlayers = 2; }
    else if (role === "client") { slot = 1; maxPlayers = 2; }
  }

  if (
    url.pathname !== "/ws" ||
    !/^[A-Z0-9]{2,12}$/.test(roomCode) ||
    maxPlayers === 0 ||
    slot < 0 ||
    slot >= maxPlayers ||
    typeof wsKey !== "string" ||
    wsKey.length === 0
  ) {
    socket.end("HTTP/1.1 400 Bad Request\r\n\r\n");
    return;
  }

  const playerName = sanitizeName(url.searchParams.get("name"), `Player ${slot + 1}`);
  const lbCode = sanitizeLbCode(url.searchParams.get("lb"));

  const accept = crypto
    .createHash("sha1")
    .update(wsKey + WS_MAGIC)
    .digest("base64");

  socket.setNoDelay(true);
  socket.write(
    "HTTP/1.1 101 Switching Protocols\r\n" +
      "Upgrade: websocket\r\n" +
      "Connection: Upgrade\r\n" +
      `Sec-WebSocket-Accept: ${accept}\r\n\r\n`
  );

  const room = getOrCreateRoom(roomCode, maxPlayers, lbCode);
  if (slot === 0) {
    room.maxPlayers = maxPlayers;
    if (lbCode) room.leaderboardCode = lbCode;
    for (let i = room.maxPlayers; i < 4; i++) {
      if (room.slots[i]) {
        room.slots[i].closeWithCode(1000);
        room.slots[i] = null;
      }
    }
  } else if (lbCode && !room.leaderboardCode) {
    room.leaderboardCode = lbCode;
  }

  const conn = new WsConn(socket, roomCode, slot);
  const previous = room.slots[slot];
  room.slots[slot] = conn;
  room.names[slot] = playerName;
  if (previous) {
    previous.closeWithCode(1000);
  }

  console.log(`[Relay] Room ${roomCode}: slot ${slot} ("${playerName}") joined (max=${room.maxPlayers}, lb=${room.leaderboardCode || "-"})`);
  sendRoomStatus(room);

  socket.on("data", (chunk) => {
    conn.rxBuf = conn.rxBuf.length > 0 ? Buffer.concat([conn.rxBuf, chunk]) : chunk;
    processRxBuffer(conn);
  });

  socket.on("end", () => detachConnection(conn));
  socket.on("close", () => detachConnection(conn));
  socket.on("error", () => {
    detachConnection(conn);
    conn.destroy();
  });

  if (head && head.length > 0) {
    conn.rxBuf = Buffer.from(head);
    processRxBuffer(conn);
  }
});

setInterval(() => {
  for (const room of rooms.values()) {
    for (const c of room.slots) {
      if (c) c.sendPing();
    }
  }
}, PING_INTERVAL_MS);

server.listen(PORT, "0.0.0.0", () => {
  console.log(`[Relay] Peggle Nights v2 (1v1/2v2 + Leaderboards) listening on 0.0.0.0:${PORT}`);
});
