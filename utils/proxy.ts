import * as net from 'net';

// Configuration
const LOCAL_HOST = process.env.LOCAL_HOST || '0.0.0.0';
const LOCAL_PORT = parseInt(process.env.LOCAL_PORT || '1883', 10);
const REMOTE_HOST = process.env.REMOTE_HOST || 'raspberrypi';
const REMOTE_PORT = parseInt(process.env.REMOTE_PORT || '1883', 10);
const LOG_DATA = process.env.LOG_DATA === 'true';

const server = net.createServer((clientSocket) => {
    const clientIdentifier = `${clientSocket.remoteAddress}:${clientSocket.remotePort}`;
    console.log(`[${new Date().toISOString()}] [${clientIdentifier}] Client connected.`);

    const remoteSocket = new net.Socket();

    remoteSocket.connect(REMOTE_PORT, REMOTE_HOST, () => {
        console.log(`[${new Date().toISOString()}] [${clientIdentifier}] Successfully connected to remote: ${REMOTE_HOST}:${REMOTE_PORT}`);
    });

    clientSocket.on('data', (data) => {
        if (LOG_DATA) {
            console.log(`[${new Date().toISOString()}] [${clientIdentifier}] C -> R (${data.length} bytes)`);
            // Log data as hex to see control packets, formatted for readability
            console.log(data.toString('hex').match(/.{1,32}/g)?.join('\n'));
        }
        const flushed = remoteSocket.write(data);
        if (!flushed) {
            console.log(`[${new Date().toISOString()}] [${clientIdentifier}] R buffer is full. Pausing C socket.`);
            clientSocket.pause();
        }
    });

    remoteSocket.on('data', (data) => {
        if (LOG_DATA) {
            console.log(`[${new Date().toISOString()}] [${clientIdentifier}] R -> C (${data.length} bytes)`);
            console.log(data.toString('hex').match(/.{1,32}/g)?.join('\n'));
        }
        const flushed = clientSocket.write(data);
        if (!flushed) {
            console.log(`[${new Date().toISOString()}] [${clientIdentifier}] C buffer is full. Pausing R socket.`);
            remoteSocket.pause();
        }
    });

    clientSocket.on('drain', () => {
        console.log(`[${new Date().toISOString()}] [${clientIdentifier}] C buffer drained. Resuming R socket.`);
        remoteSocket.resume();
    });

    remoteSocket.on('drain', () => {
        console.log(`[${new Date().toISOString()}] [${clientIdentifier}] R buffer drained. Resuming C socket.`);
        clientSocket.resume();
    });

    clientSocket.on('close', (hadError) => {
        console.log(`[${new Date().toISOString()}] [${clientIdentifier}] Client disconnected ${hadError ? 'with an error' : 'gracefully'}.`);
        if (!remoteSocket.destroyed) {
            remoteSocket.destroy();
        }
    });

    remoteSocket.on('close', (hadError) => {
        console.log(`[${new Date().toISOString()}] [${clientIdentifier}] Remote disconnected ${hadError ? 'with an error' : 'gracefully'}.`);
        if (!clientSocket.destroyed) {
            clientSocket.destroy();
        }
    });

    clientSocket.on('error', (err) => {
        console.error(`[${new Date().toISOString()}] [${clientIdentifier}] Client socket error: ${err.message}`);
    });

    remoteSocket.on('error', (err) => {
        console.error(`[${new Date().toISOString()}] [${clientIdentifier}] Remote socket error: ${err.message}`);
    });
});

server.on('error', (err) => {
    console.error(`[${new Date().toISOString()}] Server error:`, err.message);
});

server.listen(LOCAL_PORT, LOCAL_HOST, () => {
    console.log(`[${new Date().toISOString()}] TCP proxy listening on ${LOCAL_HOST}:${LOCAL_PORT}`);
    console.log(`[${new Date().toISOString()}] Forwarding traffic to ${REMOTE_HOST}:${REMOTE_PORT}`);
});
