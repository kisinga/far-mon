package config

import (
	"strings"

	"github.com/spf13/viper"
)

// Config stores all configuration for the application.
type Config struct {
	ThingsBoard ThingsBoardConfig
}

// ThingsBoardConfig stores configuration for connecting to ThingsBoard.
type ThingsBoardConfig struct {
	Host  string
	Port  int
	Token string
}

// LoadConfig reads configuration from file or environment variables.
func LoadConfig() (config Config, err error) {
	viper.AddConfigPath(".")
	viper.SetConfigName("config")
	viper.SetConfigType("yaml")
	viper.SetEnvKeyReplacer(strings.NewReplacer(".", "_"))
	viper.AutomaticEnv()

	err = viper.ReadInConfig()
	if err != nil {
		if _, ok := err.(viper.ConfigFileNotFoundError); !ok {
			return
		}
	}

	err = viper.Unmarshal(&config)
	return
}
