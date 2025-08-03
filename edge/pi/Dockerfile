# Dockerfile for Raspberry Pi services
FROM golang:1.18-alpine

WORKDIR /app

# Copy the configuration file
COPY config.yaml .

COPY src/go.mod src/go.sum ./
RUN go mod download

COPY src/ .

# Build the relay-bridge binary
RUN go build -o /relay-bridge ./cmd/relay-bridge

# Set the entrypoint
CMD ["/relay-bridge"]
