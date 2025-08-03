package thingsboard

import (
	"encoding/json"
	"farm/edge/pi/src/pkg/config"
	"fmt"
	"log"

	mqtt "github.com/eclipse/paho.mqtt.golang"
)

// Client is a client for the ThingsBoard API.
type Client struct {
	config config.ThingsBoardConfig
	client mqtt.Client
}

// CommandHandler is a function that handles commands from ThingsBoard.
type CommandHandler func(device string, command string, params map[string]interface{})

// NewClient creates a new ThingsBoard client.
func NewClient(config config.ThingsBoardConfig) *Client {
	return &Client{config: config}
}

// Connect connects to the MQTT broker.
func (c *Client) Connect(handler CommandHandler) error {
	opts := mqtt.NewClientOptions()
	opts.AddBroker(fmt.Sprintf("tcp://%s:%d", c.config.Host, c.config.Port))
	opts.SetUsername(c.config.Token)
	opts.SetOnConnectHandler(func(client mqtt.Client) {
		log.Println("Connected to ThingsBoard MQTT")
		// Subscribe to RPC requests
		token := client.Subscribe("v1/devices/me/rpc/request/+", 1, func(client mqtt.Client, msg mqtt.Message) {
			log.Printf("Received RPC request on topic %s: %s\n", msg.Topic(), msg.Payload())
			var data struct {
				Method string                 `json:"method"`
				Params map[string]interface{} `json:"params"`
			}
			if err := json.Unmarshal(msg.Payload(), &data); err != nil {
				log.Printf("Error unmarshalling RPC request: %v", err)
				return
			}
			// In a real implementation, you would extract the device from the topic
			handler("some-device", data.Method, data.Params)
		})
		token.Wait()
		if token.Error() != nil {
			log.Printf("Error subscribing to RPC topic: %v", token.Error())
		}
	})

	c.client = mqtt.NewClient(opts)
	if token := c.client.Connect(); token.Wait() && token.Error() != nil {
		return token.Error()
	}
	return nil
}

// SendTelemetry sends telemetry data to ThingsBoard.
func (c *Client) SendTelemetry(data string) error {
	// Implementation to send data to ThingsBoard would go here.
	token := c.client.Publish("v1/devices/me/telemetry", 0, false, data)
	token.Wait()
	return token.Error()
}
