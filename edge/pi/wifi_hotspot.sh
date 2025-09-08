#!/bin/bash

# --- Terminal Colors (ANSI escape codes) ---
# No external dependencies needed; these are standard for terminals.
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
MAGENTA='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color (reset)
BOLD='\033[1m'

# --- Variables ---
# Allow overrides via environment or simple .env-style file next to this script
ENV_FILE="$(dirname "$0")/.wifi_hotspot.env"
if [[ -f "$ENV_FILE" ]]; then
  # shellcheck disable=SC1090
  source "$ENV_FILE"
fi

SSID="${SSID:-PiHotspot}"
PASSWORD="${PASSWORD:-ChangeMe-$(tr -dc 'A-Za-z0-9' </dev/urandom 2>/dev/null | head -c 12 || echo 12345678)}"
INTERFACE="${INTERFACE:-wlan0}" # Default wireless interface

# --- Helper Functions ---

# Function to display error messages
log_error() {
    echo -e "${RED}[ERROR]${NC} $1" >&2
}

# Function to display informational messages
log_info() {
    echo -e "${CYAN}[INFO]${NC} $1"
}

# Function to display success messages
log_success() {
    echo -e "${GREEN}[OK]${NC} $1"
}

# Function to display warning messages
log_warning() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

# Function to check if a command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# --- Main Functions ---

# Function to check connected devices
check_connected_devices() {
    echo -e "${BOLD}${BLUE}=== Connected Devices ===${NC}"
    echo -e "${CYAN}Current Time:${NC} $(date)"
    echo ""

    # Get hotspot IP
    HOTSPOT_IP=$(ip addr show "$INTERFACE" | grep "inet " | awk '{print $2}' | cut -d'/' -f1)
    if [ -z "$HOTSPOT_IP" ]; then
        log_warning "Hotspot is not active on interface '$INTERFACE'."
        return
    fi

    # Determine the network prefix (e.g., 10.42.0) from the hotspot IP
    HOTSPOT_NET_PREFIX=$(echo "$HOTSPOT_IP" | awk -F'.' '{print $1"."$2"."$3}')

    log_info "Hotspot IP: ${BOLD}${HOTSPOT_IP}${NC}"
    echo ""

    DEVICE_COUNT=0
    local -a devices_info # Array to store device details for later processing

    # Read ARP table entries for the specified interface, excluding incomplete ones.
    # Use process substitution to allow DEVICE_COUNT to update in the parent shell.
    while IFS= read -r line; do
        # Example ARP output format: "? (10.42.0.100) at 00:11:22:33:44:55 [ether] on wlan0"
        # Extract IP, MAC, and Hostname using awk for robustness
        IP_PAREN=$(echo "$line" | awk '{print $2}') # e.g., (10.42.0.100)
        MAC=$(echo "$line" | awk '{print $4}')      # e.g., 00:11:22:33:44:55
        HOSTNAME=$(echo "$line" | awk '{print $1}') # e.g., '?' or 'mydevice'

        # Clean the IP by removing parentheses
        IP=$(echo "$IP_PAREN" | sed 's/[()]//g')

        # Skip if the IP is empty or if it's the hotspot's own IP
        if [ -z "$IP" ] || [ "$IP" = "$HOTSPOT_IP" ]; then
            continue
        fi

        # Ensure the IP belongs to the hotspot's network prefix
        if [[ ! "$IP" =~ ^"${HOTSPOT_NET_PREFIX}"\.[0-9]+$ ]]; then
            continue
        fi

        # If hostname is just '?' or the IP itself, clear it for cleaner display
        if [ "$HOSTNAME" = "?" ] || [ "$HOSTNAME" = "$IP_PAREN" ]; then
            HOSTNAME="N/A"
        fi

        # Store device info
        devices_info+=("$IP|$MAC|$HOSTNAME")

    done < <(arp -a | grep "on $INTERFACE" | grep -v "incomplete")

    if [ ${#devices_info[@]} -eq 0 ]; then
        log_warning "No active devices found on '$INTERFACE'."
    fi

    # Loop through collected devices and display their status
    for device_entry in "${devices_info[@]}"; do
        IFS='|' read -r current_ip current_mac current_hostname <<< "$device_entry"

        echo -e "${BOLD}${GREEN}- Device:${NC} ${current_ip}"
        echo -e "  ${YELLOW}L-- MAC: ${current_mac}${NC}"
        echo -e "  ${YELLOW}L-- Name: ${current_hostname}${NC}"

        # Ping the device to check online status
        if ping -c 1 -W 1 "$current_ip" >/dev/null 2>&1; then
            echo -e "  ${GREEN}L-- Status: ONLINE${NC}"
        else
            echo -e "  ${RED}L-- Status: OFFLINE${NC}"
        fi
        echo "" # Add a blank line for separation
        DEVICE_COUNT=$((DEVICE_COUNT + 1))
    done


    echo -e "${BOLD}${BLUE}--- Summary ---${NC}"
    log_info "Total devices connected: ${BOLD}${DEVICE_COUNT}${NC}"
}

# Function to set up the hotspot
setup_hotspot() {
    echo -e "${BOLD}${BLUE}=== Setting up Hotspot ===${NC}"
    echo ""

    # Check for NetworkManager command
    if ! command_exists nmcli; then
        log_info "NetworkManager (nmcli) not found. Attempting to install..."
        if ! command_exists apt; then
            log_error "apt command not found. Cannot install NetworkManager. Please install it manually."
            exit 1
        fi
        sudo apt update -qq || { log_error "Failed to update apt. Check internet or repositories."; exit 1; }
        sudo apt install -y network-manager || { log_error "Failed to install NetworkManager."; exit 1; }
        log_success "NetworkManager installed."
    fi

    # Stop conflicting services (dhcpcd is common on Raspberry Pi OS Lite)
    log_info "Stopping conflicting services (dhcpcd)..."
    sudo systemctl stop dhcpcd &>/dev/null
    sudo systemctl disable dhcpcd &>/dev/null
    log_success "Conflicting services stopped/disabled (if running)."

    # Enable and start NetworkManager
    log_info "Starting NetworkManager service..."
    sudo systemctl enable NetworkManager &>/dev/null || { log_error "Failed to enable NetworkManager."; exit 1; }
    sudo systemctl start NetworkManager || { log_error "Failed to start NetworkManager."; exit 1; }
    log_success "NetworkManager is running."

    # Delete existing hotspot connections with the same SSID or 'Hotspot' default
    log_info "Deleting existing hotspot connections (if any)..."
    sudo nmcli connection delete Hotspot &>/dev/null || true
    sudo nmcli connection delete "$SSID" &>/dev/null || true
    log_success "Existing connections removed."

    # Create hotspot
    log_info "Creating hotspot '${BOLD}$SSID${NC}'..."
    if ! sudo nmcli device wifi hotspot ssid "$SSID" password "$PASSWORD"; then
        log_error "Failed to create hotspot. Check interface '$INTERFACE' or NetworkManager status."
        exit 1
    fi
    log_success "Hotspot created successfully."

    # Configure hotspot to auto-connect on boot
    log_info "Configuring hotspot to auto-connect on boot..."
    
    # Get the name of the active connection on the interface.
    # This is the most reliable way to get the connection name created by 'hotspot' command.
    HOTSPOT_NAME=$(nmcli -g GENERAL.CONNECTION device show "$INTERFACE" 2>/dev/null)

    if [ -z "$HOTSPOT_NAME" ]; then
        log_warning "Could not determine the active hotspot connection name on '$INTERFACE'. Auto-connect might not be set."
    else
        log_info "Identified hotspot connection: '${BOLD}$HOTSPOT_NAME${NC}'"
        sudo nmcli connection modify "$HOTSPOT_NAME" connection.autoconnect yes || {
            log_error "Failed to configure '$HOTSPOT_NAME' for auto-connect. Manual configuration may be needed."
        }
        log_success "'${BOLD}$HOTSPOT_NAME${NC}' configured for auto-connect."
    fi

    echo ""
    echo -e "${BOLD}${BLUE}--- Hotspot Setup Complete! ---${NC}"
    log_info "SSID: ${BOLD}${SSID}${NC}"
    if [[ -f "$ENV_FILE" ]]; then
        log_info "Password: ${BOLD}(from ${ENV_FILE})${NC}"
    else
        log_warning "Password auto-generated for this session. Create ${ENV_FILE} to persist:"
        echo "SSID='${SSID}'" | sudo tee "$ENV_FILE" >/dev/null
        echo "PASSWORD='${PASSWORD}'" | sudo tee -a "$ENV_FILE" >/dev/null
        echo "INTERFACE='${INTERFACE}'" | sudo tee -a "$ENV_FILE" >/dev/null
        sudo chmod 600 "$ENV_FILE" || true
        log_success "Saved credentials to ${ENV_FILE} (600)"
    fi
    echo ""
    echo -e "${YELLOW}To check connected devices, run:${NC} ${BOLD}$0 check${NC}"
    echo ""
}

# --- Main Script Logic ---

# Check for root privileges
if [[ $EUID -ne 0 ]]; then
    log_error "This script requires root privileges. Please run with 'sudo'."
    echo "Example: sudo $0 setup"
    exit 1
fi

case "${1:-}" in
    setup)
        setup_hotspot
        ;;
    check)
        check_connected_devices
        ;;
    *)
        echo -e "${BOLD}${MAGENTA}Usage:${NC} $0 {setup|check}"
        echo ""
        echo -e "${BOLD}Commands:${NC}"
        echo -e "  ${GREEN}setup${NC}  - Create and configure the Wi-Fi hotspot."
        echo -e "  ${GREEN}check${NC}  - Check and list devices currently connected to the hotspot."
        echo ""
        exit 1
        ;;
esac
