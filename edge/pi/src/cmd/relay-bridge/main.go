package main

import (
	"farm/edge/pi/src/pkg/config"
	"farm/edge/pi/src/pkg/serial"
	"farm/edge/pi/src/pkg/thingsboard"
	"fmt"
	"log"
)

func main() {
	cfg, err := config.LoadConfig()
	if err != nil {
		log.Fatalf("Failed to load configuration: %v", err)
	}

	tbClient := thingsboard.NewClient(cfg.ThingsBoard)

	fmt.Println("Starting relay-bridge...")
	for {
		data, err := serial.Read()
		if err != nil {
			log.Printf("Error reading from serial: %v", err)
			continue
		}

		if err := tbClient.SendTelemetry(data); err != nil {
			log.Printf("Error sending telemetry to ThingsBoard: %v", err)
		}
	}
}
