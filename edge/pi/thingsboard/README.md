# edge/pi/thingsboard/ — ThingsBoard Deployment

- Runs as a Docker service on the Pi.
- Provides local telemetry dashboard, rule engine, and device registry.
- Secured by Tailscale VPN; not accessible from public Internet.

## Getting Started

1. Install Docker & Docker Compose.
2. Copy provided `docker-compose.yml` to this directory.
3. Start ThingsBoard:  
   ```sh
   docker compose up -d
