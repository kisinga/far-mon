#pragma once

#include <stdint.h>

class IPersistenceHal {
public:
    virtual ~IPersistenceHal() = default;

    virtual bool begin(const char* namespace_name) = 0;
    virtual void end() = 0;
    virtual bool saveU32(const char* key, uint32_t value) = 0;
    virtual uint32_t loadU32(const char* key, uint32_t defaultValue = 0) = 0;
};

#include <Preferences.h>

class FlashPersistenceHal : public IPersistenceHal {
public:
    FlashPersistenceHal() = default;

    bool begin(const char* namespace_name) override {
        return preferences.begin(namespace_name, false); // false = not read-only
    }

    void end() override {
        preferences.end();
    }

    bool saveU32(const char* key, uint32_t value) override {
        return preferences.putUInt(key, value) > 0;
    }

    uint32_t loadU32(const char* key, uint32_t defaultValue = 0) override {
        return preferences.getUInt(key, defaultValue);
    }

private:
    Preferences preferences;
};
