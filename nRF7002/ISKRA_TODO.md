1. TODO bluetooth
dane od Kamila powinny byc wpisane do src\data.c (zrobiony jest interfejs do wpisywania danych)
sprawdzić czy pokazują się na serwerze http
2. jak skonfigurować http
config servera http (uart):
    wifi cred add -s "Iskra" -k 1 -p "12345678"
    wifi cred auto_connect
po tym powinno połączyć można to wklepać pare razy
3. config socketów:
powershell on desktop:
    PS C:\Users\fiskra> Set-Location "C:\Users\fiskra\studia\ai_szponters\node-socket-server"
    PS C:\Users\fiskra\studia\ai_szponters\node-socket-server> $env:HOST="127.0.0.1"
    PS C:\Users\fiskra\studia\ai_szponters\node-socket-server> $env:PORT="19000"
    PS C:\Users\fiskra\studia\ai_szponters\node-socket-server> & "C:\Program Files\nodejs\node.exe" server.js
    [server] fatal error: listen EADDRINUSE: address already in use 127.0.0.1:19000
    PS C:\Users\fiskra\studia\ai_szponters\node-socket-server> $env:PORT="19001"
    PS C:\Users\fiskra\studia\ai_szponters\node-socket-server> & "C:\Program Files\nodejs\node.exe" server.js
    [server] listening on 127.0.0.1:19001
    [server] waiting for board TCP client...
in files:
    config APP_NODE_SOCKET_SERVER_IPV4
	string "Node server IPv4 address"
	default "172.20.10.2" - podmienić na adres IP kompa