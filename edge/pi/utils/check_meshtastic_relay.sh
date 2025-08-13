#!/bin/bash
# check_meshtastic_relay.sh - Comprehensive Meshtastic relay status checker
#
# This script shows:
# - Hotspot status and configuration
# - Tailscale connection and route approval status
# - All connected devices with their IPs and MAC addresses
# - Network interface details
# - Troubleshooting hints

echo "=== Meshtastic Relay Status Check ==="
echo "$(date)"
echo ""

# Check hotspot status
echo "📶 HOTSPOT STATUS:"
if nmcli connection show --active | grep -q -i hotspot; then
    HOTSPOT_NAME=$(nmcli connection show --active | grep -i hotspot | awk '{print $1}')
    echo "   ✓ Active: $HOTSPOT_NAME"
    
    # Get hotspot details
    SSID=$(nmcli connection show "$HOTSPOT_NAME" | grep "802-11-wireless.ssid" | awk -F: '{print $2}' | xargs)
    echo "   📡 SSID: $SSID"
    echo "   🔑 Password: awesome33"
    
    # Check if auto-connect is enabled
    AUTOCONNECT=$(nmcli connection show "$HOTSPOT_NAME" | grep "connection.autoconnect:" | awk '{print $2}')
    if [ "$AUTOCONNECT" = "yes" ]; then
        echo "   🚀 Auto-start: Enabled"
    else
        echo "   ⚠️  Auto-start: Disabled"
    fi
else
    echo "   ❌ Hotspot not running"
    echo "   💡 Run: ./reset_relay.sh to fix"
fi

echo ""

# Check Tailscale status
echo "🌐 TAILSCALE STATUS:"
if tailscale status >/dev/null 2>&1; then
    TAILSCALE_IP=$(tailscale ip -4)
    echo "   ✓ Connected: $TAILSCALE_IP"
    
    # Check if subnet routes are approved
    if tailscale status --json 2>/dev/null | grep -q "10.42.0.0/24" || tailscale status | grep -q "10.42.0.0/24"; then
        echo "   ✅ Subnet routes: Approved and active"
    else
        echo "   ⚠️  Subnet routes: NOT APPROVED"
        echo "   🔗 Fix at: https://login.tailscale.com/admin/machines"
    fi
    
    # Show Tailscale device name
    DEVICE_NAME=$(tailscale status --json 2>/dev/null | grep -o '"Name":"[^"]*"' | head -1 | cut -d'"' -f4 || echo "Unknown")
    echo "   🏷️  Device name: $DEVICE_NAME"
else
    echo "   ❌ Not connected"
    echo "   💡 Run: sudo tailscale up"
fi

echo ""

# Check IP forwarding
echo "🔀 IP FORWARDING:"
IP_FORWARD=$(sysctl net.ipv4.ip_forward | awk '{print $3}')
if [ "$IP_FORWARD" = "1" ]; then
    echo "   ✓ Enabled"
else
    echo "   ❌ Disabled"
    echo "   💡 Run: sudo sysctl -w net.ipv4.ip_forward=1"
fi

echo ""

# Show network interfaces and IPs
echo "🔌 NETWORK INTERFACES:"
ip addr show | grep -E "^[0-9]+:|inet " | while read line; do
    if [[ $line =~ ^[0-9]+: ]]; then
        INTERFACE=$(echo "$line" | awk '{print $2}' | sed 's/://')
        STATE=$(echo "$line" | grep -o "state [A-Z]*" | awk '{print $2}')
        echo "   📡 $INTERFACE ($STATE)"
    elif [[ $line =~ inet ]]; then
        IP=$(echo "$line" | awk '{print $2}')
        echo "      └─ $IP"
    fi
done

echo ""

# Find and display all connected devices
echo "📱 CONNECTED DEVICES:"

# Get gateway IP (should be 10.42.0.1)
GATEWAY_IP=$(ip route | grep "10.42.0" | grep "proto kernel" | awk '{print $9}' | head -1)
if [ -n "$GATEWAY_IP" ]; then
    echo "   🏠 Gateway: $GATEWAY_IP (this device)"
else
    echo "   ⚠️  No 10.42.0.x gateway found"
fi

# Scan for devices on the 10.42.0.0/24 network
echo "   🔍 Scanning for devices..."

# Method 1: Check ARP table for known devices
ARP_DEVICES=$(arp -a | grep "10.42.0" | grep -v "incomplete")
if [ -n "$ARP_DEVICES" ]; then
    echo "$ARP_DEVICES" | while read line; do
        # Parse ARP entry: hostname (IP) at MAC [ether] on interface
        IP=$(echo "$line" | grep -o "10\.42\.0\.[0-9]*")
        MAC=$(echo "$line" | grep -o "[0-9a-f][0-9a-f]:[0-9a-f][0-9a-f]:[0-9a-f][0-9a-f]:[0-9a-f][0-9a-f]:[0-9a-f][0-9a-f]:[0-9a-f][0-9a-f]")
        HOSTNAME=$(echo "$line" | awk '{print $1}' | sed 's/[()]//g')
        
        if [ "$IP" != "$GATEWAY_IP" ]; then
            echo "   📲 Device: $IP"
            echo "      └─ MAC: $MAC"
            if [ "$HOSTNAME" != "$IP" ]; then
                echo "      └─ Name: $HOSTNAME"
            fi
            
            # Try to ping to check if device is responsive
            if ping -c 1 -W 1 "$IP" >/dev/null 2>&1; then
                echo "      └─ Status: ✅ Responsive"
            else
                echo "      └─ Status: ⚠️  Not responding to ping"
            fi
        fi
    done
else
    echo "   📭 No devices found in ARP table"
fi

# Method 2: Quick nmap scan if available and no ARP entries
if command -v nmap >/dev/null 2>&1 && [ -z "$ARP_DEVICES" ]; then
    echo "   🔍 Running network scan..."
    NMAP_RESULTS=$(nmap -sn 10.42.0.0/24 2>/dev/null | grep "Nmap scan report" | grep -v "$GATEWAY_IP")
    if [ -n "$NMAP_RESULTS" ]; then
        echo "$NMAP_RESULTS" | while read line; do
            IP=$(echo "$line" | grep -o "10\.42\.0\.[0-9]*")
            echo "   📲 Device: $IP (found via scan)"
        done
    fi
fi

# Count total devices (excluding gateway)
DEVICE_COUNT=$(arp -a | grep "10.42.0" | grep -v "incomplete" | grep -v "$GATEWAY_IP" | wc -l)
echo ""
echo "📊 SUMMARY:"
echo "   Total connected devices: $DEVICE_COUNT"

# Show DHCP lease info if available
if [ -f /var/lib/dhcp/dhcpd.leases ]; then
    ACTIVE_LEASES=$(grep -c "binding state active" /var/lib/dhcp/dhcpd.leases 2>/dev/null || echo "0")
    echo "   Active DHCP leases: $ACTIVE_LEASES"
fi

echo ""

# Provide troubleshooting hints based on status
echo "💡 TROUBLESHOOTING:"
if ! nmcli connection show --active | grep -q -i hotspot; then
    echo "   • Hotspot not running → Run: ./reset_relay.sh"
fi

if ! tailscale status >/dev/null 2>&1; then
    echo "   • Tailscale disconnected → Run: sudo tailscale up"
fi

if tailscale status >/dev/null 2>&1 && ! (tailscale status --json 2>/dev/null | grep -q "10.42.0.0/24" || tailscale status | grep -q "10.42.0.0/24"); then
    echo "   • Routes not approved → Visit: https://login.tailscale.com/admin/machines"
fi

if [ "$DEVICE_COUNT" -eq 0 ]; then
    echo "   • No devices connected → Check WiFi password: awesome33"
    echo "   • Devices may take a few minutes to appear in ARP table"
fi

echo ""
echo "🔗 Remote access: Use device IPs (10.42.0.x) from any Tailscale-connected device"
echo "📋 Helper scripts: ./reset_relay.sh (reset everything)"
