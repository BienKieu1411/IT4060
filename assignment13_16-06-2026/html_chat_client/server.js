/*
 * HTML Chat Client Bridge
 * Browser HTML cannot connect directly to a raw TCP socket.
 * This Node.js bridge connects:
 *   Browser <-> WebSocket <-> Node.js <-> TCP Chat Server
 *
 * Run:
 *   npm install
 *   npm start
 *
 * Open:
 *   http://localhost:8080
 */

const http = require("http");
const fs = require("fs");
const path = require("path");
const net = require("net");
const WebSocket = require("ws");

const WEB_PORT = process.env.WEB_PORT || 8080;
const PUBLIC_DIR = path.join(__dirname, "public");

function sendJson(ws, obj) {
  if (ws.readyState === WebSocket.OPEN) {
    ws.send(JSON.stringify(obj));
  }
}

function contentType(filePath) {
  if (filePath.endsWith(".html")) return "text/html; charset=utf-8";
  if (filePath.endsWith(".css")) return "text/css; charset=utf-8";
  if (filePath.endsWith(".js")) return "application/javascript; charset=utf-8";
  if (filePath.endsWith(".png")) return "image/png";
  if (filePath.endsWith(".jpg") || filePath.endsWith(".jpeg")) return "image/jpeg";
  return "text/plain; charset=utf-8";
}

const server = http.createServer((req, res) => {
  let reqPath = decodeURIComponent(req.url.split("?")[0]);
  if (reqPath === "/") reqPath = "/index.html";

  const filePath = path.normalize(path.join(PUBLIC_DIR, reqPath));
  if (!filePath.startsWith(PUBLIC_DIR)) {
    res.writeHead(403);
    res.end("Forbidden");
    return;
  }

  fs.readFile(filePath, (err, data) => {
    if (err) {
      res.writeHead(404);
      res.end("Not found");
      return;
    }
    res.writeHead(200, { "Content-Type": contentType(filePath) });
    res.end(data);
  });
});

const wss = new WebSocket.Server({ server });

wss.on("connection", (ws) => {
  let tcp = null;
  let tcpBuffer = "";

  function closeTcp() {
    if (tcp) {
      tcp.destroy();
      tcp = null;
    }
  }

  ws.on("message", (raw) => {
    let msg;
    try {
      msg = JSON.parse(raw.toString());
    } catch {
      sendJson(ws, { type: "error", message: "Invalid JSON from browser" });
      return;
    }

    if (msg.type === "connect") {
      closeTcp();

      const host = String(msg.host || "127.0.0.1").trim();
      const port = Number(msg.port || 9000);
      const nick = String(msg.nick || "").trim();

      if (!nick) {
        sendJson(ws, { type: "error", message: "Nickname is required" });
        return;
      }

      tcp = net.createConnection({ host, port }, () => {
        sendJson(ws, { type: "status", message: `Connected to ${host}:${port}` });
        tcp.write(`JOIN ${nick}\n`);
      });

      tcp.setEncoding("utf8");

      tcp.on("data", (chunk) => {
        tcpBuffer += chunk;
        let pos;
        while ((pos = tcpBuffer.indexOf("\n")) >= 0) {
          const line = tcpBuffer.slice(0, pos + 1);
          tcpBuffer = tcpBuffer.slice(pos + 1);
          sendJson(ws, { type: "server", line });
        }
      });

      tcp.on("error", (err) => {
        sendJson(ws, { type: "error", message: err.message });
      });

      tcp.on("close", () => {
        sendJson(ws, { type: "status", message: "TCP server connection closed" });
        tcp = null;
      });

      return;
    }

    if (msg.type === "send") {
      if (!tcp) {
        sendJson(ws, { type: "error", message: "Not connected to chat server" });
        return;
      }

      let line = String(msg.line || "");
      if (!line.endsWith("\n")) line += "\n";
      tcp.write(line);
      return;
    }

    if (msg.type === "disconnect") {
      if (tcp) tcp.write("QUIT\n");
      closeTcp();
      sendJson(ws, { type: "status", message: "Disconnected" });
      return;
    }
  });

  ws.on("close", () => {
    closeTcp();
  });
});

server.listen(WEB_PORT, () => {
  console.log(`HTML chat client running at http://localhost:${WEB_PORT}`);
});
