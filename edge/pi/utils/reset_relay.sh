#!/bin/bash
# reset_relay.sh - Reset and restart the Meshtastic relay system
#
# This script will:
# - Stop existing hotspot connections
# - Reset network manager configurations
# - Restart the WiFi hotspot with correct settings
# - Restart Tailscale with proper routing
# - Reset IP forwarding
# - Restart DHCP services if needed

echo "=== Resetting Meshtastic Relay System ==="
echo "$(date)"
echo ""

# Function to check if running as root
check_root() {
    if [[ $EUID -ne 0 ]]; then
        echo "❌ This script requires root privileges"
        echo "💡 Please run: sudo $0"
        exit 1
    fi
}

# Function to stop all hotspot connections
stop_hotspots() {
    echo "🛑 Stopping existing hotspot connections..."
    
    # Find and stop any active hotspot connections
    ACTIVE_HOTSPOTS=$(nmcli connection show --active | grep -i hotspot | awk '{print $1}' || true)
    
    if [ -n "$ACTIVE_HOTSPOTS" ]; then
        echo "$ACTIVE_HOTSPOTS" | while read hotspot; do
            echo "   Stopping: $hotspot"
            nmcli connection down "$hotspot" 2>/dev/null || true
        done
    else
        echo "   No active hotspots found"
    fi
    
    # NetworkManager handles hotspot internally - no separate daemons to kill
}

# Function to delete old hotspot configurations
cleanup_configs() {
    echo "🧹 Cleaning up old configurations..."
    
    # Remove any existing MESHRELAY connections
    nmcli connection delete "MESHRELAY" 2>/dev/null || true
    nmcli connection delete "Hotspot" 2>/dev/null || true
    
    # Clean up any stale WiFi configurations
    OLD_HOTSPOTS=$(nmcli connection show | grep -i "hotspot\|meshrelay" | awk '{print $1}' || true)
    if [ -n "$OLD_HOTSPOTS" ]; then
        echo "$OLD_HOTSPOTS" | while read hotspot; do
            echo "   Removing: $hotspot"
            nmcli connection delete "$hotspot" 2>/dev/null || true
        done
    fi
}

# Function to create fresh hotspot
create_hotspot() {
    echo "📶 Creating new WiFi hotspot..."
    
    # Get the WiFi interface (usually wlan0)
    WIFI_INTERFACE=$(ip link show | grep -E "wl[a-z0-9]+" | head -1 | awk '{print $2}' | sed 's/://')
    
    if [ -z "$WIFI_INTERFACE" ]; then
        echo "   ❌ No WiFi interface found"
        return 1
    fi
    
    echo "   📡 Using interface: $WIFI_INTERFACE"
    
    # Create the hotspot connection
    nmcli connection add type wifi ifname "$WIFI_INTERFACE" \
        con-name "MESHRELAY" \
        autoconnect yes \
        wifi.mode ap \
        wifi.ssid "MESHRELAY" \
        ipv4.method shared \
        ipv4.address 10.42.0.1/24 \
        wifi.security wpa-psk \
        wifi.psk "awesome33"
    
    if [ $? -eq 0 ]; then
        echo "   ✓ Hotspot configuration created"
    else
        echo "   ❌ Failed to create hotspot configuration"
        return 1
    fi
    
    # Start the hotspot
    echo "   🚀 Starting hotspot..."
    nmcli connection up "MESHRELAY"
    
    if [ $? -eq 0 ]; then
        echo "   ✓ Hotspot started successfully"
    else
        echo "   ❌ Failed to start hotspot"
        return 1
    fi
}

# Function to configure IP forwarding
setup_ip_forwarding() {
    echo "🔀 Configuring IP forwarding..."
    
    # Enable IP forwarding immediately
    sysctl -w net.ipv4.ip_forward=1
    
    # Make it persistent across reboots
    if ! grep -q "net.ipv4.ip_forward=1" /etc/sysctl.conf; then
        echo "net.ipv4.ip_forward=1" >> /etc/sysctl.conf
        echo "   ✓ IP forwarding enabled permanently"
    else
        echo "   ✓ IP forwarding already configured in sysctl.conf"
    fi
}

# Function to restart Tailscale with proper routing
restart_tailscale() {
    echo "🌐 Restarting Tailscale with subnet routing..."
    
    # Stop Tailscale
    tailscale down 2>/dev/null || true
    
    # Wait a moment
    sleep 2
    
    # Start Tailscale with subnet routing
    tailscale up --advertise-routes=10.42.0.0/24 --accept-routes
    
    if [ $? -eq 0 ]; then
        echo "   ✓ Tailscale restarted with subnet routing"
        echo "   🔗 Don't forget to approve routes at: https://login.tailscale.com/admin/machines"
    else
        echo "   ❌ Failed to restart Tailscale"
        echo "   💡 Try manually: sudo tailscale up --advertise-routes=10.42.0.0/24 --accept-routes"
    fi
}

# Function to restart networking services
restart_services() {
    echo "🔄 Restarting networking services..."
    
    # Restart NetworkManager to ensure clean state
    systemctl restart NetworkManager
    echo "   ✓ NetworkManager restarted"
    
    # Wait for NetworkManager to stabilize
    sleep 5
    
    # Restart systemd-resolved if present
    if systemctl is-active --quiet systemd-resolved; then
        systemctl restart systemd-resolved
        echo "   ✓ systemd-resolved restarted"
    fi
}

# Function to verify setup
verify_setup() {
    echo "🔍 Verifying setup..."
    
    # Check if hotspot is running
    if nmcli connection show --active | grep -q "MESHRELAY"; then
        echo "   ✅ Hotspot is active"
    else
        echo "   ❌ Hotspot is not active"
    fi
    
    # Check IP forwarding
    if [ "$(sysctl net.ipv4.ip_forward | awk '{print $3}')" = "1" ]; then
        echo "   ✅ IP forwarding enabled"
    else
        echo "   ❌ IP forwarding disabled"
    fi
    
    # Check Tailscale
    if tailscale status >/dev/null 2>&1; then
        echo "   ✅ Tailscale connected"
    else
        echo "   ❌ Tailscale not connected"
    fi
    
    # Check gateway IP
    GATEWAY_IP=$(ip route | grep "10.42.0" | grep "proto kernel" | awk '{print $9}' | head -1)
    if [ "$GATEWAY_IP" = "10.42.0.1" ]; then
        echo "   ✅ Gateway IP configured correctly: $GATEWAY_IP"
    else
        echo "   ⚠️  Gateway IP issue. Expected: 10.42.0.1, Got: $GATEWAY_IP"
    fi
}

# Main execution
main() {
    check_root
    
    echo "This will reset your Meshtastic relay configuration."
    echo "WiFi hotspot will be recreated with:"
    echo "  • SSID: MESHRELAY"
    echo "  • Password: awesome33"
    echo "  • Network: 10.42.0.0/24"
    echo ""
    
    read -p "Continue? (y/N): " -n 1 -r
    echo ""
    
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "❌ Reset cancelled"
        exit 0
    fi
    
    stop_hotspots
    cleanup_configs
    restart_services
    create_hotspot
    setup_ip_forwarding
    restart_tailscale
    
    echo ""
    verify_setup
    
    echo ""
    echo "🎉 Reset complete!"
    echo ""
    echo "Next steps:"
    echo "1. Go to https://login.tailscale.com/admin/machines"
    echo "2. Find this device and approve the 10.42.0.0/24 route"
    echo "3. Connect your Meshtastic devices to WiFi: MESHRELAY / awesome33"
    echo "4. Run ./check_meshtastic_relay.sh to verify everything is working"
    echo ""
}

# Run main function
main "$@"
