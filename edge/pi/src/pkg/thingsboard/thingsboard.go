package thingsboard

import (
	"farm/edge/pi/src/pkg/config"
	"fmt"
)

// Client is a client for the ThingsBoard API.
type Client struct {
	config.ThingsBoardConfig
}

// NewClient creates a new ThingsBoard client.
func NewClient(config config.ThingsBoardConfig) *Client {
	return &Client{config}
}

// SendTelemetry sends telemetry data to ThingsBoard.
func (c *Client) SendTelemetry(data string) error {
	fmt.Printf("Sending to %s:%d: %s\n", c.Host, c.Port, data)
	// Implementation to send data to ThingsBoard would go here.
	return nil
}
