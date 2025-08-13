#!/bin/bash
# setup_farm_pi.sh - Complete farm monitoring Pi setup
#
# This script sets up the foundational infrastructure for a farm monitoring Pi:
# 1. Tailscale VPN for secure remote access
# 2. Coolify for container orchestration and deployment
# 3. Basic system hardening and prerequisites
#
# After this setup, deploy the farm stack (Node-RED, Mosquitto, InfluxDB) via Coolify.
# Then optionally add Meshtastic relay functionality.
#
# Usage:
#   curl -fsSL https://raw.githubusercontent.com/your-org/farm/main/edge/pi/utils/setup_farm_pi.sh | sudo bash
#
# Requirements:
# - Raspberry Pi with internet connection
# - Raspberry Pi OS (Debian-based)
# - Static IP configured for MQTT stability

set -e  # Exit on any error

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

# Configuration
REPO_URL="https://github.com/your-org/farm.git"  # Update with actual repo URL
INSTALL_DIR="/opt/farm"

echo "🚜 Farm Monitoring Pi Setup"
echo "============================"
echo "Setting up foundational infrastructure:"
echo "• Tailscale VPN for secure access"
echo "• Coolify for service orchestration"
echo "• System hardening and prerequisites"
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
    log_info "Checking system requirements..."
    
    # Check if we're on a Debian-based system
    if ! command -v apt >/dev/null 2>&1; then
        log_error "This script requires a Debian-based system (apt package manager)"
        exit 1
    fi
    
    # Check internet connectivity
    if ! ping -c 1 google.com >/dev/null 2>&1; then
        log_error "No internet connection. Please connect to internet before running setup."
        exit 1
    fi
    
    # Check if we have a static IP (important for MQTT)
    IP_METHOD=$(nmcli -t -f ipv4.method connection show "$(nmcli -t -f name connection show --active | head -1)" | cut -d: -f2)
    if [ "$IP_METHOD" != "manual" ]; then
        log_warning "No static IP detected. MQTT brokers require static IPs for Meshtastic devices."
        log_warning "Consider setting a static IP: sudo nmcli connection modify <name> ipv4.method manual ipv4.addresses <ip/mask>"
    else
        log_success "Static IP configuration detected"
    fi
    
    log_success "System requirements verified"
}

# Function to update system packages
update_system() {
    log_info "Updating system packages..."
    apt update
    apt upgrade -y
    log_success "System packages updated"
}

# Function to install prerequisites
install_prerequisites() {
    log_info "Installing prerequisites..."
    
    # Essential packages
    PACKAGES=(
        "git"               # For repository management
        "curl"              # For downloads
        "htop"              # System monitoring
        "vim"               # Text editor
        "unzip"             # Archive extraction
        "ca-certificates"   # SSL certificates
        "gnupg"             # GPG keys
        "lsb-release"       # System info
        "apt-transport-https" # HTTPS repositories
    )
    
    for package in "${PACKAGES[@]}"; do
        log_info "Installing $package..."
        apt install -y "$package"
    done
    
    log_success "Prerequisites installed"
}

# Function to configure system hardening
configure_system() {
    log_info "Configuring system settings..."
    
    # Set timezone
    timedatectl set-timezone Africa/Nairobi
    log_info "Timezone set to Africa/Nairobi"
    
    # Enable time synchronization
    systemctl enable systemd-timesyncd
    systemctl start systemd-timesyncd
    log_info "Time synchronization enabled"
    
    # Configure memory split (for headless Pi)
    if command -v raspi-config >/dev/null 2>&1; then
        raspi-config nonint do_memory_split 16
        log_info "GPU memory split optimized for headless operation"
    fi
    
    # Disable swap to reduce SD card wear
    if systemctl is-active --quiet dphys-swapfile; then
        systemctl stop dphys-swapfile
        systemctl disable dphys-swapfile
        log_info "Swap disabled to protect SD card"
    fi
    
    # Enable hardware watchdog
    if [ -f /boot/config.txt ]; then
        if ! grep -q "dtparam=watchdog=on" /boot/config.txt; then
            echo "dtparam=watchdog=on" >> /boot/config.txt
            log_info "Hardware watchdog enabled"
        fi
    fi
    
    log_success "System configuration completed"
}

# Function to install Docker
install_docker() {
    log_info "Installing Docker..."
    
    # Check if Docker is already installed
    if command -v docker >/dev/null 2>&1; then
        log_warning "Docker already installed"
        return 0
    fi
    
    # Install Docker using official script
    curl -fsSL https://get.docker.com | sh
    
    # Add user to docker group
    usermod -aG docker "$ACTUAL_USER"
    
    # Enable and start Docker
    systemctl enable docker
    systemctl start docker
    
    # Install docker-compose
    curl -L "https://github.com/docker/compose/releases/latest/download/docker-compose-$(uname -s)-$(uname -m)" -o /usr/local/bin/docker-compose
    chmod +x /usr/local/bin/docker-compose
    
    log_success "Docker installed and configured"
}

# Function to install Tailscale
install_tailscale() {
    log_info "Installing Tailscale..."
    
    # Check if Tailscale is already installed
    if command -v tailscale >/dev/null 2>&1; then
        log_warning "Tailscale already installed"
        return 0
    fi
    
    # Download and install Tailscale
    curl -fsSL https://tailscale.com/install.sh | sh
    
    log_success "Tailscale installed"
}

# Function to setup Tailscale
setup_tailscale() {
    log_info "Setting up Tailscale..."
    
    echo ""
    echo "🌐 Tailscale Setup"
    echo "=================="
    echo "Tailscale provides secure remote access to your farm Pi."
    echo ""
    echo "Setup options:"
    echo "1. Interactive login (recommended for first-time setup)"
    echo "2. Auth key (for automated deployments)"
    echo ""
    
    read -p "Do you want to run interactive Tailscale setup now? (y/N): " -n 1 -r
    echo ""
    
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        log_info "Starting Tailscale interactive setup..."
        tailscale up --ssh --hostname "farm-pi"
        
        if [ $? -eq 0 ]; then
            TAILSCALE_IP=$(tailscale ip -4 2>/dev/null || echo "Unknown")
            log_success "Tailscale setup completed"
            log_info "Tailscale IP: $TAILSCALE_IP"
            log_info "SSH access enabled via: tailscale ssh $ACTUAL_USER@farm-pi"
        else
            log_warning "Tailscale setup encountered issues. You can complete it manually later."
        fi
    else
        log_info "Skipping interactive setup. Complete Tailscale setup later with:"
        echo "  sudo tailscale up --ssh --hostname farm-pi"
        echo "  Or with auth key: sudo tailscale up --authkey=<your-key> --ssh --hostname farm-pi"
    fi
}

# Function to install Coolify
install_coolify() {
    log_info "Installing Coolify..."
    
    # Check if Coolify is already installed
    if [ -d "/data/coolify" ]; then
        log_warning "Coolify appears to be already installed"
        return 0
    fi
    
    # Create coolify user if it doesn't exist
    if ! id "coolify" &>/dev/null; then
        useradd -m -s /bin/bash coolify
        usermod -aG docker coolify
        log_info "Created coolify user"
    fi
    
    # Install Coolify
    curl -fsSL https://cdn.coollabs.io/coolify/install.sh | bash
    
    log_success "Coolify installation completed"
}

# Function to setup repository
setup_repository() {
    log_info "Setting up farm repository..."
    
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
    
    # Set proper ownership
    chown -R "$ACTUAL_USER:$ACTUAL_USER" "$INSTALL_DIR"
    
    log_success "Repository setup completed"
}

# Function to create service directories
create_service_dirs() {
    log_info "Creating service directories..."
    
    # Create directories for core farm services
    mkdir -p /srv/{mosquitto/{config,data,log},nodered-data,influx,influx-config,backups}
    
    # Create optional service directories (for ThingsBoard if needed)
    mkdir -p /srv/{thingsboard,thingsboard-logs,postgres}
    
    # Set ownership for core services
    chown -R 1883:1883 /srv/mosquitto
    chown -R 1000:1000 /srv/nodered-data
    chown -R "$ACTUAL_USER:$ACTUAL_USER" /srv/{influx,influx-config,backups}
    
    # Set ownership for optional services (ready if enabled later)
    chown -R 70:70 /srv/postgres  # PostgreSQL user
    chown -R "$ACTUAL_USER:$ACTUAL_USER" /srv/{thingsboard,thingsboard-logs}
    
    log_success "Service directories created with proper ownership"
}

# Function to verify setup
verify_setup() {
    log_info "Verifying setup..."
    
    # Check Docker
    if docker --version >/dev/null 2>&1; then
        log_success "Docker is working"
    else
        log_warning "Docker may have issues"
    fi
    
    # Check Tailscale
    if tailscale status >/dev/null 2>&1; then
        log_success "Tailscale is connected"
    else
        log_warning "Tailscale not connected (this is OK if you skipped setup)"
    fi
    
    # Check Coolify
    if systemctl is-active --quiet coolify; then
        log_success "Coolify service is running"
    else
        log_warning "Coolify service may not be running yet"
    fi
}

# Function to show final instructions
show_final_instructions() {
    echo ""
    echo "🎉 Farm Pi Foundation Setup Complete!"
    echo "====================================="
    echo ""
    echo "✅ Installed:"
    echo "   • Docker & Docker Compose"
    echo "   • Tailscale VPN"
    echo "   • Coolify orchestration platform"
    echo "   • Farm repository at $INSTALL_DIR"
    echo ""
    echo "🚀 Next Steps:"
    echo ""
    echo "1. **Complete Tailscale Setup** (if not done):"
    echo "   sudo tailscale up --ssh --hostname farm-pi"
    echo ""
    echo "2. **Access Coolify Dashboard:**"
    
    # Try to get the Pi's IP
    LOCAL_IP=$(ip route get 1.1.1.1 | grep -oP 'src \K[^ ]+' 2>/dev/null || echo "YOUR_PI_IP")
    
    if tailscale status >/dev/null 2>&1; then
        TAILSCALE_IP=$(tailscale ip -4 2>/dev/null || echo "")
        if [ -n "$TAILSCALE_IP" ]; then
            echo "   Via Tailscale: http://$TAILSCALE_IP:8000"
        fi
    fi
    echo "   Via local network: http://$LOCAL_IP:8000"
    echo ""
    echo "3. **Deploy Farm Stack via Coolify:**"
    echo "   • Add this Pi as a server in Coolify"
    echo "   • Deploy docker-compose.yml from $INSTALL_DIR/edge/pi/"
    echo "   • Services: Mosquitto (1883), Node-RED (1880), InfluxDB (8086)"
    echo ""
    echo "4. **Add Meshtastic Relay (optional):**
    echo "   $INSTALL_DIR/edge/pi/utils/install_meshtastic_relay.sh"
    echo ""
    echo "🔧 Management:"
    echo "   • Coolify: http://$LOCAL_IP:8000"
    echo "   • SSH via Tailscale: tailscale ssh $ACTUAL_USER@farm-pi"
    echo "   • Repository: $INSTALL_DIR"
    echo ""
    
    if [ "$ACTUAL_USER" != "root" ]; then
        echo "⚠️  **Important**: Log out and back in for Docker group membership to take effect"
        echo "   Or run: newgrp docker"
    fi
    
    log_success "Setup completed! Follow the next steps to deploy your farm monitoring stack."
}

# Main execution
main() {
    check_requirements
    update_system
    install_prerequisites
    configure_system
    install_docker
    install_tailscale
    setup_tailscale
    install_coolify
    setup_repository
    create_service_dirs
    verify_setup
    show_final_instructions
}

# Run main function
main "$@"
