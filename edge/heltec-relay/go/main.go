package main

import (
	"image/color"
	"machine"
	"time"

	"tinygo.org/x/drivers/lora"
	"tinygo.org/x/drivers/ssd1306"
	"tinygo.org/x/drivers/sx126x"
	"tinygo.org/x/tinyfont"
	"tinygo.org/x/tinyfont/freemono"
)

// Heltec WiFi LoRa 32 (V3) pin configuration
const (
	// LoRa-specific pins for SX1262
	loraSCK  = machine.GPIO9
	loraSDI  = machine.GPIO11 // MISO
	loraSDO  = machine.GPIO10 // MOSI
	loraCS   = machine.GPIO8
	loraRST  = machine.GPIO12
	loraDIO  = machine.GPIO14 // DIO1
	loraBUSY = machine.GPIO13

	// OLED-specific pins
	oledSDA = machine.GPIO17
	oledSCL = machine.GPIO18
	oledRST = machine.GPIO21
)

var (
	// UART for communication with the Raspberry Pi
	uart = machine.DefaultUART

	// LoRa radio
	loraRadio *sx126x.Device

	// OLED display
	display ssd1306.Device
)

func main() {
	// Give the serial monitor a moment to connect.
	time.Sleep(2 * time.Second)

	println("Heltec Relay Node (V3) starting...")

	// Initialize UART
	uart.Configure(machine.UARTConfig{
		BaudRate: 9600,
		// For ESP32-S3, the default UART pins are usually correct
	})

	// Initialize OLED
	initOLED()
	displayStatus("Relay Starting...")

	// Initialize LoRa radio
	initLoRa()

	println("Relay node started.")
	displayStatus("Relay Started")

	for {
		// Check for incoming LoRa packets
		if size, _ := loraRadio.Receive(lora.Read); size > 0 {
			buffer := make([]byte, size)
			loraRadio.Read(buffer)

			if len(buffer) > 0 && buffer[0] == 0xFF {
				handleStatusPacket(buffer)
			} else {
				uart.Write(buffer)
				println("Forwarded LoRa packet to serial")
				displayStatus("LoRa -> Serial")
			}
		}

		// Check for incoming serial data
		if uart.Buffered() > 0 {
			var serialBuffer []byte
			for uart.Buffered() > 0 {
				data, _ := uart.ReadByte()
				serialBuffer = append(serialBuffer, data)
			}

			if len(serialBuffer) > 0 {
				loraRadio.Send(serialBuffer, 0)
				println("Broadcasted serial data to LoRa")
				displayStatus("Serial -> LoRa")
			}
		}

		time.Sleep(10 * time.Millisecond)
	}
}

func initOLED() {
	machine.I2C0.Configure(machine.I2CConfig{
		SDA: oledSDA,
		SCL: oledSCL,
	})
	display = ssd1306.NewI2C(machine.I2C0)
	display.Configure(ssd1306.Config{
		Width:  128,
		Height: 64,
	})
	display.ClearDisplay()
}

func initLoRa() {
	machine.SPI0.Configure(machine.SPIConfig{
		Frequency: 500000,
		SCK:       loraSCK,
		SDI:       loraSDI,
		SDO:       loraSDO,
	})

	loraRadio = sx126x.New(machine.SPI0, loraCS, loraRST, loraDIO, loraBUSY)

	// Configure LoRa
	loraConf := lora.Config{
		Freq:           915000000, // 915 MHz
		Bw:             lora.Bandwidth_125_0,
		Sf:             lora.SpreadingFactor9,
		Cr:             lora.CodingRate4_7,
		HeaderType:     lora.HeaderExplicit,
		Preamble:       12,
		Ldo:            lora.LdoOn,
		Iq:             lora.IQStandard,
		Crc:            lora.CRCOn,
		SyncWord:       lora.SyncPrivate,
		LoraTxPowerDBm: 20,
	}
	err := loraRadio.Configure(loraConf)
	if err != nil {
		println("Failed to configure LoRa:", err)
		displayStatus("LoRa Fail")
		for {
		}
	}
}

func handleStatusPacket(packet []byte) {
	println("Received status packet:")
	displayStatus("Status Rcvd")
	// ... (rest of the function)
}

func displayStatus(text string) {
	display.ClearBuffer()
	tinyfont.WriteLine(&display, &freemono.Bold9pt7b, 10, 20, "Status:", color.RGBA{255, 255, 255, 255})
	tinyfont.WriteLine(&display, &freemono.Bold9pt7b, 10, 40, text, color.RGBA{255, 255, 255, 255})
	display.Display()
}
