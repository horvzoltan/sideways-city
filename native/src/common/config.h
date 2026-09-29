// config.h - tiny key = value file: settings (shared by the launcher and the game) and save data.
#pragma once
#include <map>
#include <string>

class Config {
public:
    bool Load(const std::string& path);
    bool Save(const std::string& path) const;
    std::string Get(const std::string& k, const std::string& def) const;
    float GetFloat(const std::string& k, float def) const;
    int GetInt(const std::string& k, int def) const;
    bool GetBool(const std::string& k, bool def) const;
    void Set(const std::string& k, const std::string& v) { values[k] = v; }
    void SetFloat(const std::string& k, float v);
    void SetInt(const std::string& k, int v) { values[k] = std::to_string(v); }
    void SetBool(const std::string& k, bool v) { values[k] = v ? "1" : "0"; }
    // Command line overrides:  +key value
    void ApplyArgs(int argc, char** argv);
    std::map<std::string, std::string> values;
};

// Defaults used by both programs.
void SetConfigDefaults(Config& c);
