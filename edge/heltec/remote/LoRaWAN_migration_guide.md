# Remote Device LoRaWAN Migration Guide

## Overview

This guide provides a step-by-step migration path for the remote device to transition from the custom LoRa protocol to LoRaWAN. The migration maintains API compatibility where possible while leveraging LoRaWAN's superior reliability and features.

## Key Changes

### 1. Protocol Migration

**Before (LoRa):**

- Custom peer-to-peer protocol with master/slave architecture
- Direct device addressing using 8-bit device IDs
- Manual ACK/retry logic
- Connection state managed through peer presence tracking

**After (LoRaWAN):**

- Standard LoRaWAN Class A protocol
- Network server handles addressing and routing
- Built-in confirmed/unconfirmed message types
- Connection state managed through join status

### 2. API Compatibility Layer

The new `ILoRaWANService` and `ILoRaWANHal` interfaces provide compatibility with the existing codebase:

- `sendData(port, payload, length, confirmed)` instead of `sendData(destId, payload, length, requireAck)`
- `isConnected()` and `getConnectionState()` work similarly
- `getLastRssiDbm()` provides signal quality information
- `getPeerCount()` always returns 1 (the gateway)

## Implementation Steps

### Step 1: Include New Headers

```cpp
// Replace these includes:
#include "lib/hal_lora.h"
#include "lib/svc_lora.h"

// With these:
#include "lib/hal_lorawan.h"
#include "lib/svc_lorawan.h"
```

### Step 2: Update HAL Initialization

```cpp
// Replace LoRa HAL creation:
std::unique_ptr<ILoRaHal> loraHal = std::make_unique<LoRaCommHal>();

// With LoRaWAN HAL creation:
std::unique_ptr<ILoRaWANHal> lorawanHal = std::make_unique<LoRaWANHal>();

// Initialize with keys instead of mode/device ID:
uint8_t devEui[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01}; // Unique device ID
uint8_t appEui[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01}; // Application ID
uint8_t appKey[16] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01}; // App session key

lorawanHal->begin(devEui, appEui, appKey);
```

### Step 3: Update Service Creation

```cpp
// Replace LoRa service creation:
std::unique_ptr<ILoRaService> loraService = std::make_unique<LoRaService>(*loraHal);

// With LoRaWAN service creation:
std::unique_ptr<ILoRaWANService> lorawanService = std::make_unique<LoRaWANService>(*lorawanHal);

// Update service references throughout the code
commsService->setLoraHal(lorawanHal.get()); // Keep this for compatibility
```

### Step 4: Update Data Transmission

```cpp
// Replace destination-based sending:
// loraService->sendData(masterNodeId, payload, length, true);

// With port-based sending:
// lorawanService->sendData(1, payload, length, false); // Port 1, unconfirmed

// For commands that need confirmation:
// lorawanService->sendData(2, commandPayload, length, true); // Port 2, confirmed
```

### Step 5: Update Connection Management

```cpp
// Replace master node ID setting:
// loraHal->setMasterNodeId(config.masterNodeId);

// LoRaWAN manages connections automatically - no equivalent needed

// Replace forced reconnection:
// loraService->forceReconnect();

// With LoRaWAN reconnection:
lorawanService->forceReconnect(); // This will trigger a new join attempt
```

### Step 6: Update Status Display

```cpp
// Update UI status reporting:
// Replace:
loraStatusElement->setLoraStatus(isConnected, loraService->getLastRssiDbm());

// With:
auto connectionState = lorawanService->getConnectionState();
bool isConnected = (connectionState == ILoRaWANService::ConnectionState::Connected);
loraStatusElement->setLoraStatus(isConnected, lorawanService->getLastRssiDbm());

// Also display SNR for better signal quality indication:
statusTextElement->setText(String("SNR: ") + String(lorawanService->getLastSnr()) + " dB");
```

### Step 7: Update Scheduler Tasks

```cpp
// Replace LoRa task:
// scheduler.registerTask("lora", [this](CommonAppState& state){
//     loraService->update(state.nowMs);
//     // ... existing UI updates
// }, 50);

// With LoRaWAN task:
scheduler.registerTask("lorawan", [this](CommonAppState& state){
    lorawanService->update(state.nowMs);
    // ... existing UI updates
}, 50);

// Replace watchdog task:
// if (state.nowMs - lastSuccessfulAckMs > config.maxQuietTimeMs) {
//     loraService->forceReconnect();
// }

// With LoRaWAN watchdog:
if (!lorawanService->isConnected() && lorawanService->isJoined()) {
    lorawanService->forceReconnect();
}
```

### Step 8: Update Sensor Transmission

```cpp
// The LoRaBatchTransmitter needs minimal changes:
// Replace loraHal reference with lorawanService
auto transmitter = std::make_unique<LoRaBatchTransmitter>(lorawanService.get(), config);

// Update transmission calls:
// sensorTransmitter->queueBatch(readings);
// if (lorawanService->isConnected()) {
//     sensorTransmitter->update(state.nowMs);
// }
```

## Configuration Changes

### Remote Config Updates

Add LoRaWAN-specific configuration to `RemoteConfig`:

```cpp
struct LoRaWANConfig {
    uint8_t devEui[8];
    uint8_t appEui[8];
    uint8_t appKey[16];
    uint8_t defaultPort = 1;
    bool useConfirmedMessages = false;
    bool enableADR = true;
};

LoRaWANConfig lorawan;
```

### Key Management

**Important:** Each remote device needs unique LoRaWAN keys:

- **DevEUI**: Unique 8-byte device identifier
- **AppEUI**: 8-byte application identifier (can be shared across devices)
- **AppKey**: 16-byte application key (can be shared across devices)

Generate keys using:

```bash
# Generate random keys (example)
openssl rand -hex 8  # for DevEUI
openssl rand -hex 8  # for AppEUI
openssl rand -hex 16 # for AppKey
```

## Testing Strategy

### Phase 1: Parallel Testing

1. Deploy both LoRa and LoRaWAN implementations
2. Compare message delivery rates
3. Monitor connection stability
4. Validate command reception

### Phase 2: Gradual Migration

1. Migrate devices one by one
2. Monitor network performance
3. Validate backward compatibility
4. Test failover scenarios

## Troubleshooting

### Common Issues

**Join Failures:**

- Verify key correctness
- Check antenna connection
- Monitor gateway coverage

**Message Loss:**

- Enable confirmed messages for critical data
- Monitor duty cycle limits
- Check network server logs

**Connection Drops:**

- Monitor signal quality (RSSI/SNR)
- Check for interference
- Verify power supply stability

## Rollback Plan

If issues arise:

1. Revert to LoRa implementation
2. Keep LoRaWAN code for future deployment
3. Analyze failure patterns
4. Implement fixes before retrying migration

## Benefits of Migration

1. **Improved Reliability**: LoRaWAN's built-in retry and confirmation mechanisms
2. **Better Signal Quality**: SNR reporting provides better diagnostics
3. **Remote Management**: Downlink commands enable remote configuration
4. **Standards Compliance**: Interoperability with standard LoRaWAN infrastructure
5. **Future-Proof**: Easier integration with commercial LoRaWAN networks
