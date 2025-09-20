#include "HeaderStatusElement.h"

HeaderStatusElement::HeaderStatusElement() {}

void HeaderStatusElement::setMode(Mode mode) {
    _mode = mode;
}

void HeaderStatusElement::setLoraStatus(bool connected, int16_t rssi) {
    _loraConnected = connected;
    _loraRssi = rssi;
}

void HeaderStatusElement::setWifiStatus(bool connected, int8_t signalStrength) {
    _wifiConnected = connected;
    _wifiSignalStrength = signalStrength;
}

void HeaderStatusElement::setPeerCount(uint16_t count) {
    _peerCount = count;
}

void HeaderStatusElement::draw(SSD1306Wire& display, int x, int y, int w, int h) {
    // The drawing functions are hardcoded to the top right of the screen.
    // The x, y, w, h from the layout are ignored.
    switch (_mode) {
        case Mode::Lora:
            drawLoraSignal(display);
            break;
        case Mode::Wifi:
            drawWifiStatus(display);
            break;
        case Mode::PeerCount:
            drawPeerCount(display);
            break;
    }
}

// Logic copied from OledDisplay::drawLoraSignal
void HeaderStatusElement::drawLoraSignal(SSD1306Wire &d) {
    const int16_t topY = 0;
    const int16_t headerH = 10;
    const int8_t bars = 4;
    const int8_t barWidth = 2;
    const int8_t barGap = 1;
    const int8_t maxBarHeight = headerH - 2;
    const int16_t totalWidth = bars * barWidth + (bars - 1) * barGap;
    int16_t startX = d.width() - totalWidth;
    
    uint8_t level = 0;
    if (_loraConnected) {
        if (_loraRssi < -115) level = 1;
        else if (_loraRssi < -105) level = 2;
        else if (_loraRssi < -95) level = 3;
        else level = 4;
    }

    for (int i = 0; i < bars; i++) {
        int16_t x = startX + i * (barWidth + barGap);
        int8_t h = (int8_t)((i + 1) * maxBarHeight / bars);
        int16_t y = topY + (maxBarHeight - h);
        if (i < level) {
            d.fillRect(x, y, barWidth, h);
        } else {
            d.drawRect(x, y, barWidth, h);
        }
    }
}

// Logic copied from OledDisplay::drawWifiStatus
void HeaderStatusElement::drawWifiStatus(SSD1306Wire &d) {
    const int16_t topY = 0;
    const int16_t headerH = 10;
    const int16_t iconW = 14;
    const int16_t startX = d.width() - iconW;
    const int16_t cx = startX + iconW / 2;
    const int16_t cy = topY + headerH - 1;

    auto plotUpperArc = [&](int r) {
        for (int x = -r; x <= r; x++) {
            int y = (int)sqrtf((float)((r * r) - (x * x)));
            d.setPixel(cx + x, cy - y);
            if (y > 0) d.setPixel(cx + x, cy - y - 1);
        }
    };

    if (!_wifiConnected) {
        plotUpperArc(6);
        plotUpperArc(4);
        plotUpperArc(2);
        d.fillRect(cx, cy - 1, 2, 2); // dot
        d.drawLine(startX, topY + 1, startX + iconW - 1, topY + headerH - 2); // slash
        return;
    }

    uint8_t level = 0;
    if (_wifiSignalStrength >= 0) {
        if (_wifiSignalStrength <= 33) level = 1;
        else if (_wifiSignalStrength <= 66) level = 2;
        else level = 3;
    }

    d.fillRect(cx, cy - 1, 2, 2); // dot
    if (level >= 1) plotUpperArc(2);
    if (level >= 2) plotUpperArc(4);
    if (level >= 3) plotUpperArc(6);
}

// Logic copied from OledDisplay::drawPeersCount
void HeaderStatusElement::drawPeerCount(SSD1306Wire &d) {
    d.setTextAlignment(TEXT_ALIGN_RIGHT);
    d.drawString(d.width(), 0, String("P:") + String((uint32_t)_peerCount));
}
