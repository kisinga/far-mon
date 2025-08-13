#!/bin/bash
# install_meshtastic_relay.sh - Add Meshtastic Heltec access relay to existing farm Pi
#
# This script adds Meshtastic relay functionality to an existing farm monitoring Pi.
# It assumes you already have:
# - Tailscale VPN configured and connected
# - Coolify running with farm services (Mosquitto, Node-RED, InfluxDB)
# - Basic Pi setup completed
#
# This adds:
# - WiFi hotspot for Heltec devices to connect to
# - Network bridge allowing access to connected devices from Tailscale network
# - Integration with existing farm MQTT broker
#
# Usage:
#   # After setting up farm Pi with setup_farm_pi.sh and deploying services via Coolify:
#   sudo ./install_meshtastic_relay.sh
#
# What this creates:
# - WiFi Hotspot: MESHRELAY / awesome33
# - Network: 10.42.0.0/24 (Pi at 10.42.0.1)
# - Subnet routing via existing Tailscale connection
# - Remote access to Heltec devices at 10.42.0.x IPs
#
# Requirements:
# - Farm Pi already setup with Tailscale + Coolify + services
# - WiFi capability (separate from main internet connection)
# - Root/sudo access

set -e  # Exit on any error

# Configuration
REPO_URL="https://github.com/your-org/farm.git"  # Update with actual repo URL
INSTALL_DIR="/opt/meshtastic-relay"
HOTSPOT_SSID="MESHRELAY"
HOTSPOT_PASSWORD="awesome33"
GATEWAY_IP="10.42.0.1"
NETWORK_RANGE="10.42.0.0/24"

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
log_info() { echo -e "${BLUE}ℹ️  $1${NC}"; }
log_success() { echo -e "${GREEN}✅ $1${NC}"; }
log_warning() { echo -e "${YELLOW}⚠️  $1${NC}"; }
log_error() { echo -e "${RED}❌ $1${NC}"; }

echo "🔧 Meshtastic Heltec Access Relay Setup"
echo "======================================"
echo "This will configure your Pi to provide remote access to Heltec devices"
echo ""
echo "Configuration:"
echo "• WiFi Hotspot: $HOTSPOT_SSID (password: $HOTSPOT_PASSWORD)"
echo "• Pi Gateway: $GATEWAY_IP"
echo "• Device Network: $NETWORK_RANGE"
echo "• Remote Access: Via Tailscale VPN"
echo ""

# Check if running as root
if [[ $EUID -ne 0 ]]; then
    log_error "This script requires root privileges"
    echo "💡 Please run: sudo $0"
    exit 1
fi

# Detect the actual user (in case running via sudo)
ACTUAL_USER=${SUDO_USER:-$(whoami)}
if [ "$ACTUAL_USER" = "root" ]; then
    ACTUAL_USER="pi"  # Default to pi user
fi
USER_HOME=$(eval echo ~$ACTUAL_USER)

log_info "Running as: $ACTUAL_USER (home: $USER_HOME)"

# Function to check system requirements
check_requirements() {
    log_info "Checking prerequisites..."
    
    # Check if Tailscale is installed and connected
    if ! command -v tailscale >/dev/null 2>&1; then
        log_error "Tailscale not found. Please run setup_farm_pi.sh first."
        exit 1
    fi
    
    if ! tailscale status >/dev/null 2>&1; then
        log_error "Tailscale not connected. Please connect to Tailscale first:"
        echo "  sudo tailscale up --ssh --hostname farm-pi"
        exit 1
    fi
    
    TAILSCALE_IP=$(tailscale ip -4 2>/dev/null || echo "")
    log_success "Tailscale connected: $TAILSCALE_IP"
    
    # Check if Docker is available (required for Coolify services)
    if ! command -v docker >/dev/null 2>&1; then
        log_error "Docker not found. Please run setup_farm_pi.sh first."
        exit 1
    fi
    
    log_success "Docker found"
    
    # Check if farm services are running (check for common containers)
    FARM_SERVICES=0
    if docker ps --format "table {{.Names}}" | grep -q -i mosquitto; then
        log_success "Mosquitto MQTT broker found"
        FARM_SERVICES=$((FARM_SERVICES + 1))
    fi
    
    if docker ps --format "table {{.Names}}" | grep -q -i nodered; then
        log_success "Node-RED found"
        FARM_SERVICES=$((FARM_SERVICES + 1))
    fi
    
    if [ $FARM_SERVICES -eq 0 ]; then
        log_warning "No farm services detected. This script works best with existing farm stack."
        log_warning "Consider deploying Mosquitto and Node-RED via Coolify first."
    fi
    
    # Check WiFi interface
    WIFI_INTERFACE=$(ip link show | grep -E "wl[a-z0-9]+" | head -1 | awk '{print $2}' | sed 's/://' || echo "")
    if [ -z "$WIFI_INTERFACE" ]; then
        log_error "No WiFi interface found. This script requires WiFi capability."
        exit 1
    fi
    
    log_success "WiFi interface found: $WIFI_INTERFACE"
    
    # Check internet connectivity
    if ! ping -c 1 google.com >/dev/null 2>&1; then
        log_error "No internet connection. Please ensure connectivity."
        exit 1
    fi
    
    log_success "Prerequisites verified"
}

# Function to install relay-specific packages
install_relay_packages() {
    log_info "Installing Meshtastic relay packages..."
    
    # Update package list
    apt update
    
    # Relay-specific packages (basics should already be installed)
    PACKAGES=(
        "nmap"              # For network scanning
        "net-tools"         # Network utilities (arp, etc.)
    )
    
    # NOTE: We use NetworkManager's built-in hotspot functionality (nmcli)
    # No need for hostapd or dnsmasq - NetworkManager handles everything
    
    for package in "${PACKAGES[@]}"; do
        if ! dpkg -l | grep -q "^ii  $package "; then
            log_info "Installing $package..."
            apt install -y "$package"
        else
            log_info "$package already installed"
        fi
    done
    
    log_success "Relay packages verified/installed"
}

# Function to clone repository and copy utilities
setup_repository() {
    log_info "Setting up repository and utilities..."
    
    # Create install directory
    mkdir -p "$INSTALL_DIR"
    
    # Clone repository if it doesn't exist
    if [ ! -d "$INSTALL_DIR/.git" ]; then
        log_info "Cloning repository..."
        git clone "$REPO_URL" "$INSTALL_DIR"
    else
        log_info "Repository exists, updating..."
        cd "$INSTALL_DIR"
        git pull
    fi
    
    # Copy utility scripts to user's home directory
    if [ -d "$INSTALL_DIR/edge/pi/utils" ]; then
        log_info "Copying utility scripts..."
        cp "$INSTALL_DIR/edge/pi/utils/check_meshtastic_relay.sh" "$USER_HOME/"
        cp "$INSTALL_DIR/edge/pi/utils/reset_relay.sh" "$USER_HOME/"
        chown "$ACTUAL_USER:$ACTUAL_USER" "$USER_HOME/check_meshtastic_relay.sh"
        chown "$ACTUAL_USER:$ACTUAL_USER" "$USER_HOME/reset_relay.sh"
        chmod +x "$USER_HOME/check_meshtastic_relay.sh"
        chmod +x "$USER_HOME/reset_relay.sh"
        
        # Create system-wide symlinks
        ln -sf "$USER_HOME/check_meshtastic_relay.sh" /usr/local/bin/check-relay
        ln -sf "$USER_HOME/reset_relay.sh" /usr/local/bin/reset-relay
        
        log_success "Utility scripts installed"
    else
        log_warning "Utility scripts not found in repository"
    fi
}

# Function to configure IP forwarding
configure_ip_forwarding() {
    log_info "Configuring IP forwarding..."
    
    # Enable IP forwarding immediately
    sysctl -w net.ipv4.ip_forward=1
    
    # Make it persistent across reboots
    if ! grep -q "net.ipv4.ip_forward=1" /etc/sysctl.conf; then
        echo "net.ipv4.ip_forward=1" >> /etc/sysctl.conf
    fi
    
    log_success "IP forwarding configured"
}

# Function to configure WiFi hotspot
configure_hotspot() {
    log_info "Configuring WiFi hotspot..."
    
    # Stop and disable any existing WiFi connections
    nmcli radio wifi off
    sleep 2
    nmcli radio wifi on
    sleep 2
    
    # Remove any existing hotspot connections
    nmcli connection delete "$HOTSPOT_SSID" 2>/dev/null || true
    
    # Create new hotspot connection
    nmcli connection add type wifi ifname "$WIFI_INTERFACE" \
        con-name "$HOTSPOT_SSID" \
        autoconnect yes \
        wifi.mode ap \
        wifi.ssid "$HOTSPOT_SSID" \
        ipv4.method shared \
        ipv4.address "$GATEWAY_IP/24" \
        wifi.security wpa-psk \
        wifi.psk "$HOTSPOT_PASSWORD"
    
    log_success "WiFi hotspot configured"
}

# Function to configure iptables for NAT
configure_iptables() {
    log_info "Configuring iptables for NAT..."
    
    # Get the internet interface (usually eth0 or the interface with default route)
    INTERNET_INTERFACE=$(ip route | grep default | awk '{print $5}' | head -1)
    
    if [ -z "$INTERNET_INTERFACE" ]; then
        log_warning "Could not determine internet interface. You may need to configure iptables manually."
        return 0
    fi
    
    log_info "Internet interface: $INTERNET_INTERFACE"
    
    # Clear existing rules
    iptables -F
    iptables -t nat -F
    
    # Set up NAT (Network Address Translation)
    iptables -t nat -A POSTROUTING -o "$INTERNET_INTERFACE" -j MASQUERADE
    iptables -A FORWARD -i "$INTERNET_INTERFACE" -o "$WIFI_INTERFACE" -m state --state RELATED,ESTABLISHED -j ACCEPT
    iptables -A FORWARD -i "$WIFI_INTERFACE" -o "$INTERNET_INTERFACE" -j ACCEPT
    
    # Allow traffic on the hotspot network
    iptables -A INPUT -i "$WIFI_INTERFACE" -j ACCEPT
    iptables -A OUTPUT -o "$WIFI_INTERFACE" -j ACCEPT
    
    # Save iptables rules
    iptables-save > /etc/iptables/rules.v4
    
    log_success "iptables configured for NAT"
}

# Function to start services
start_services() {
    log_info "Starting and enabling services..."
    
    # Enable and start NetworkManager
    systemctl enable NetworkManager
    systemctl restart NetworkManager
    
    # Wait for NetworkManager to stabilize
    sleep 5
    
    # Start the hotspot
    nmcli connection up "$HOTSPOT_SSID"
    
    # Enable IP forwarding service
    systemctl enable systemd-sysctl
    
    log_success "Services started and enabled"
}

# Function to configure Tailscale subnet routing
configure_tailscale_routing() {
    log_info "Configuring Tailscale subnet routing..."
    
    # Add subnet routing for the Meshtastic network
    log_info "Adding subnet route advertisement for $NETWORK_RANGE..."
    tailscale up --advertise-routes="$NETWORK_RANGE" --accept-routes
    
    if [ $? -eq 0 ]; then
        TAILSCALE_IP=$(tailscale ip -4 2>/dev/null || echo "Unknown")
        log_success "Tailscale subnet routing configured"
        log_info "Tailscale IP: $TAILSCALE_IP"
        log_warning "Remember to approve the $NETWORK_RANGE route in Tailscale admin panel"
    else
        log_warning "Tailscale routing configuration failed. You can try manually:"
        echo "  sudo tailscale up --advertise-routes=$NETWORK_RANGE --accept-routes"
    fi
}

# Function to verify setup
verify_setup() {
    log_info "Verifying setup..."
    
    # Check if hotspot is running
    if nmcli connection show --active | grep -q "$HOTSPOT_SSID"; then
        log_success "Hotspot is active"
    else
        log_warning "Hotspot is not active"
    fi
    
    # Check IP forwarding
    if [ "$(sysctl net.ipv4.ip_forward | awk '{print $3}')" = "1" ]; then
        log_success "IP forwarding enabled"
    else
        log_warning "IP forwarding disabled"
    fi
    
    # Check Tailscale
    if tailscale status >/dev/null 2>&1; then
        log_success "Tailscale connected"
    else
        log_warning "Tailscale not connected"
    fi
    
    # Check gateway IP
    GATEWAY_CHECK=$(ip route | grep "10.42.0" | grep "proto kernel" | awk '{print $9}' | head -1)
    if [ "$GATEWAY_CHECK" = "$GATEWAY_IP" ]; then
        log_success "Gateway IP configured correctly: $GATEWAY_CHECK"
    else
        log_warning "Gateway IP issue. Expected: $GATEWAY_IP, Got: $GATEWAY_CHECK"
    fi
}

# Function to show final instructions
show_final_instructions() {
    echo ""
    echo "🎉 Meshtastic Heltec Access Relay Setup Complete!"
    echo "================================================="
    echo ""
    echo "📋 Configuration Summary:"
    echo "   • WiFi Hotspot: $HOTSPOT_SSID"
    echo "   • Password: $HOTSPOT_PASSWORD"
    echo "   • Pi Gateway: $GATEWAY_IP"
    echo "   • Device Network: $NETWORK_RANGE"
    echo ""
    echo "🚀 Next Steps:"
    echo "1. Go to https://login.tailscale.com/admin/machines"
    echo "2. Find this Pi and approve the $NETWORK_RANGE subnet route"
    echo "3. Configure your Heltec devices to connect to WiFi: $HOTSPOT_SSID / $HOTSPOT_PASSWORD"
    echo "4. Access Heltec devices remotely using their 10.42.0.x IPs from any Tailscale device"
    echo ""
    echo "🔧 Management Commands:"
    echo "   • Check status: check-relay"
    echo "   • Reset system: reset-relay"
    echo "   • Manual Tailscale: sudo tailscale up --advertise-routes=$NETWORK_RANGE --accept-routes"
    echo ""
    echo "📡 Heltec Device Access:"
    echo "   Once connected to the $HOTSPOT_SSID network, Heltec devices will get IPs like:"
    echo "   • 10.42.0.2, 10.42.0.3, 10.42.0.4, etc."
    echo "   • Access via HTTP: http://10.42.0.x"
    echo "   • Meshtastic CLI: meshtastic --host 10.42.0.x"
    echo ""
    echo "🌐 From any Tailscale-connected device, you can now:"
    echo "   • Access this Pi: ssh $ACTUAL_USER@$GATEWAY_IP"
    echo "   • Access Heltec devices: http://10.42.0.x"
    echo "   • Monitor network: ping 10.42.0.x"
    echo ""
    
    if tailscale status >/dev/null 2>&1; then
        TAILSCALE_IP=$(tailscale ip -4 2>/dev/null || echo "Unknown")
        echo "🌐 This Pi's Tailscale IP: $TAILSCALE_IP"
        echo ""
    fi
    
    log_warning "IMPORTANT: Remember to approve the subnet route in the Tailscale admin panel!"
    log_warning "Without route approval, remote access to Heltec devices (10.42.0.x) won't work."
}

# Main execution
main() {
    check_requirements
    install_relay_packages
    setup_repository
    configure_ip_forwarding
    configure_hotspot
    configure_iptables
    start_services
    configure_tailscale_routing
    verify_setup
    show_final_instructions
}

# Run main function
main "$@"
