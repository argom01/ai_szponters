const net = require('node:net');

const BOARD_HOST = process.env.BOARD_HOST || 'httpserver.local';
const BOARD_PORT = Number(process.env.BOARD_PORT || 80);
const BOARD_PATH = process.env.BOARD_PATH || '/api/sensors.bin';
const TIMEOUT_MS = Number(process.env.TIMEOUT_MS || 5000);

const request = [
  `GET ${BOARD_PATH} HTTP/1.1`,
  `Host: ${BOARD_HOST}`,
  'Connection: close',
  '',
  ''
].join('\\r\\n');

const socket = new net.Socket();
let totalBytes = 0;
let printedHead = false;

socket.setTimeout(TIMEOUT_MS);

socket.on('connect', () => {
  console.log(`[probe] connected to ${BOARD_HOST}:${BOARD_PORT}`);
  socket.write(request);
});

socket.on('data', (chunk) => {
  totalBytes += chunk.length;

  if (!printedHead) {
    const txt = chunk.toString('utf8');
    const sep = txt.indexOf('\\r\\n\\r\\n');
    if (sep >= 0) {
      const head = txt.slice(0, sep);
      console.log('[probe] response header:');
      console.log(head);
      printedHead = true;
    }
  }
});

socket.on('timeout', () => {
  console.error(`[probe] timeout after ${TIMEOUT_MS} ms`);
  socket.destroy();
  process.exitCode = 1;
});

socket.on('error', (err) => {
  console.error('[probe] socket error:', err.message);
  process.exitCode = 1;
});

socket.on('close', (hadError) => {
  console.log(`[probe] socket closed, bytes=${totalBytes}, hadError=${hadError}`);
});

socket.connect(BOARD_PORT, BOARD_HOST);
