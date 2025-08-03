package main

import (
	"farm/edge/pi/src/pkg/config"
	"farm/edge/pi/src/pkg/serial"
	"farm/edge/pi/src/pkg/thingsboard"
	"fmt"
	"log"
	"time"
)

func main() {
	cfg, err := config.LoadConfig()
	if err != nil {
		log.Fatalf("Failed to load configuration: %v", err)
	}

	commandHandler := func(device string, command string, params map[string]interface{}) {
		log.Printf("Handling command '%s' for device '%s' with params %v\n", command, device, params)
		// Here you would translate the command and send it to the serial port.
	}

	tbClient := thingsboard.NewClient(cfg.ThingsBoard)
	if err := tbClient.Connect(commandHandler); err != nil {
		log.Fatalf("Failed to connect to ThingsBoard: %v", err)
	}

	fmt.Println("Starting relay-bridge...")
	for {
		data, err := serial.Read()
		if err != nil {
			log.Printf("Error reading from serial: %v", err)
			time.Sleep(10 * time.Second) // prevent busy-looping on serial error
			continue
		}

		if err := tbClient.SendTelemetry(data); err != nil {
			log.Printf("Error sending telemetry to ThingsBoard: %v", err)
		}
	}
}
