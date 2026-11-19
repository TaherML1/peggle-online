import http from "node:http";
import crypto from "node:crypto";

const PORT = Number(process.env.PORT || 10000);
const WS_MAGIC = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
const MAX_MSG_BYTES = 4 * 1024 * 1024;
const PING_INTERVAL_MS = 20_000;

/** @type {Map<string, { host: WsConn | null, client: WsConn | null }>} */
const rooms = new Map();

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
  constructor(socket, roomCode, role) {
    this.socket = socket;
    this.roomCode = roomCode;
    this.role = role;
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

function getOrCreateRoom(roomCode) {
  let room = rooms.get(roomCode);
  if (!room) {
    room = { host: null, client: null };
    rooms.set(roomCode, room);
  }
  return room;
}

function detachConnection(conn) {
  conn.closed = true;
  const room = rooms.get(conn.roomCode);
  if (!room) return;
  if (room[conn.role] !== conn) return;

  room[conn.role] = null;
  const otherRole = conn.role === "host" ? "client" : "host";
  const other = room[otherRole];
  if (other) {
    other.sendBinary(Buffer.from("PGBY"));
  }
  if (!room.host && !room.client) {
    rooms.delete(conn.roomCode);
  }
  console.log(`[Relay] Room ${conn.roomCode}: ${conn.role} disconnected`);
}

function handleMessage(conn, msgBuf) {
  const room = rooms.get(conn.roomCode);
  if (!room) return;
  const otherRole = conn.role === "host" ? "client" : "host";
  const other = room[otherRole];
  if (other) {
    other.sendBinary(msgBuf);
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
        return;
      }
      payloadLen = Number(bigLen);
      offset += 8;
    }

    if (payloadLen > MAX_MSG_BYTES) {
      conn.closeWithCode(1009);
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

const server = http.createServer((req, res) => {
  const url = new URL(req.url || "/", `http://${req.headers.host || "localhost"}`);
  if (req.method === "GET" && (url.pathname === "/" || url.pathname === "/health")) {
    const body = JSON.stringify({
      ok: true,
      service: "peggle-online-relay",
      activeRooms: rooms.size,
    });
    res.writeHead(200, {
      "Content-Type": "application/json",
      "Cache-Control": "no-store",
    });
    res.end(body);
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
  const role = (url.searchParams.get("role") || "").trim().toLowerCase();
  const wsKey = req.headers["sec-websocket-key"];

  if (
    url.pathname !== "/ws" ||
    !/^[A-Z0-9]{2,12}$/.test(roomCode) ||
    (role !== "host" && role !== "client") ||
    typeof wsKey !== "string" ||
    wsKey.length === 0
  ) {
    socket.end("HTTP/1.1 400 Bad Request\r\n\r\n");
    return;
  }

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

  const conn = new WsConn(socket, roomCode, role);
  const room = getOrCreateRoom(roomCode);
  const previous = room[role];
  room[role] = conn;
  if (previous) {
    previous.closeWithCode(1000);
  }

  console.log(`[Relay] Room ${roomCode}: ${role} joined`);

  if (room.host && room.client) {
    console.log(`[Relay] Room ${roomCode}: both players ready -> sending PGOK`);
    room.host.sendBinary(Buffer.from("PGOK"));
    room.client.sendBinary(Buffer.from("PGOK"));
  }

  socket.on("data", (chunk) => {
    conn.rxBuf = conn.rxBuf.length > 0 ? Buffer.concat([conn.rxBuf, chunk]) : chunk;
    processRxBuffer(conn);
  });

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
    if (room.host) room.host.sendPing();
    if (room.client) room.client.sendPing();
  }
}, PING_INTERVAL_MS);

server.listen(PORT, "0.0.0.0", () => {
  console.log(`[Relay] Peggle Nights WebSocket Room Relay listening on 0.0.0.0:${PORT}`);
});
