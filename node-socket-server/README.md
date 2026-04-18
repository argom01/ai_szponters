# Node TCP Socket Server

Simple TCP server for testing socket communication with the nRF7002 firmware.

## Run

```powershell
cd C:\\Users\\fiskra\\studia\\ai_szponters\\node-socket-server
npm start
```

## Optional env vars

- `PORT` (default: `9000`)
- `HOST` (default: `0.0.0.0`)

## Expected behavior

1. Board opens TCP connection to this server.
2. Board sends a short text payload.
3. Server logs payload and returns one-line ACK.
