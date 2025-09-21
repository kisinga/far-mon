#pragma once

class RelayApplicationImpl;

class RelayApplication {
public:
    RelayApplication();
    ~RelayApplication();

    void initialize();
    void run();

private:
    RelayApplicationImpl* impl;
};
