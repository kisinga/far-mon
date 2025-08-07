# Farm-ERP Communication Protocol

This document outlines the LoRa-based communication protocol for the Farm-ERP ecosystem, detailing the interactions between remote sensor devices and a central relay.

## 1. Overview

The protocol is designed for a star network topology where multiple **remotes** communicate with a single **relay**. Communication is packet-based, with acknowledgments for reliable message delivery.

## 2. Packet Structure

All communication is done via a standardized packet format.

```
<destination_id>:<source_id>:<packet_type>:<payload>
```

- **`destination_id`**: `uint8_t`. The ID of the recipient device. `0` is the broadcast address for the relay.
- **`source_id`**: `uint8_t`. The ID of the sending device. Each remote must have a unique ID.
- **`packet_type`**: `String`. Defines the purpose of the packet. See Packet Types.
- **`payload`**: `String`. The data content of the packet.

### Packet Types

- `SYN`: Initiates a connection request from a remote to the relay.
- `SYN-ACK`: Acknowledgment from the relay to a remote, confirming connection.
- `ACK`: A generic acknowledgment for any received packet.
- `MSG`: A standard message packet for data transmission.
- `PING`: A keep-alive message to check connection status.
- `PONG`: The response to a `PING` message.

## 3. Connection Handshake

A remote initiates a connection with the relay using a three-way handshake.

1.  **Remote to Relay**: `0:<remote_id>:SYN:CONNECT`
2.  **Relay to Remote**: `<remote_id>:<relay_id>:SYN-ACK:CONNECTED`
3.  **Remote to Relay**: `<relay_id>:<remote_id>:ACK:HANDSHAKE_COMPLETE`

After this, the remote is considered **connected**.

## 4. Communication Flow

### Remote to Relay

- A remote sends a message to the relay.
- The relay receives the message and sends an `ACK`.
- If the remote does not receive an `ACK` within a timeout period, it will re-transmit the message.

**Example:**
1. Remote: `<relay_id>:<remote_id>:MSG:temperature:25.5`
2. Relay: `<remote_id>:<relay_id>:ACK:MSG_RECEIVED`

### Relay to Remote (Broadcast)

- The relay can send a message to all connected remotes.
- Each remote that receives the broadcast message will respond with an `ACK`.

**Example:**
1. Relay: `255:<relay_id>:MSG:get_status` (assuming `255` is the broadcast address)
2. Remote 1: `<relay_id>:<remote_1_id>:ACK:STATUS_REQUEST_RECEIVED`
3. Remote 2: `<relay_id>:<remote_2_id>:ACK:STATUS_REQUEST_RECEIVED`

### Relay to Remote (Unicast)

- The relay can send a message to a specific remote.
- The targeted remote will respond with an `ACK`.

**Example:**
1. Relay: `<remote_id>:<relay_id>:MSG:reboot`
2. Remote: `<relay_id>:<remote_id>:ACK:REBOOT_COMMAND_RECEIVED`

## 5. Resilience and Reliability

- **Acknowledgments**: Every data-carrying packet (`MSG`) must be acknowledged (`ACK`).
- **Retries**: If an `ACK` is not received, the sender will re-transmit the packet up to a configurable number of times.
- **Keep-Alives**: Remotes periodically send `PING` messages to the relay to maintain the connection. If the relay doesn't receive a `PING` for a certain period, it will mark the remote as disconnected.

This protocol ensures a clear, reliable, and documented flow of communication between devices.
