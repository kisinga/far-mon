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

// LoadConfig reads configuration from file or environment variables, with fallback to defaults.
func LoadConfig() (config Config, err error) {
	// Set default values that will be compiled into the application
	viper.SetDefault("thingsboard.host", "localhost")
	viper.SetDefault("thingsboard.port", 8080)
	viper.SetDefault("thingsboard.token", "DEFAULT_TOKEN_CHANGE_ME")

	// Path for config file in Docker container. Can be overridden by mounting a volume.
	viper.AddConfigPath("/app")
	// Path for local development
	viper.AddConfigPath(".")

	viper.SetConfigName("config")
	viper.SetConfigType("yaml")

	viper.SetEnvKeyReplacer(strings.NewReplacer(".", "_"))
	viper.AutomaticEnv()

	// Attempt to read the config file. It's not an error if it doesn't exist.
	if errRead := viper.ReadInConfig(); errRead != nil {
		if _, ok := errRead.(viper.ConfigFileNotFoundError); !ok {
			// This is an actual error reading the config file.
			err = errRead
			return
		}
		// if config file is not found, viper will use defaults, which is what we want.
	}

	err = viper.Unmarshal(&config)
	return
}
