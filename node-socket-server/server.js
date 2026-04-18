const net = require('node:net');

const HOST = process.env.HOST || '0.0.0.0';
const PORT = Number(process.env.PORT || 9000);

const server = net.createServer((socket) => {
  const peer = `${socket.remoteAddress}:${socket.remotePort}`;
  console.log(`[socket] connected: ${peer}`);

  socket.setEncoding('utf8');

  socket.on('data', (chunk) => {
    const payload = String(chunk).trim();
    console.log(`[socket] rx from ${peer}: ${payload}`);

    const ack = `ACK ${new Date().toISOString()}\n`;
    socket.write(ack);
  });

  socket.on('end', () => {
    console.log(`[socket] closed by peer: ${peer}`);
  });

  socket.on('error', (err) => {
    console.error(`[socket] error ${peer}:`, err.message);
  });
});

server.on('error', (err) => {
  console.error('[server] fatal error:', err.message);
  process.exitCode = 1;
});

server.listen(PORT, HOST, () => {
  const addr = server.address();
  console.log(`[server] listening on ${addr.address}:${addr.port}`);
  console.log('[server] waiting for board TCP client...');
});
