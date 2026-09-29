#include "config.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

static std::string Trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

bool Config::Load(const std::string& path) {
    std::ifstream f(path);
    if (!f) return false;
    std::string line;
    while (std::getline(f, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == '#' || line[0] == '/') continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        values[Trim(line.substr(0, eq))] = Trim(line.substr(eq + 1));
    }
    return true;
}

bool Config::Save(const std::string& path) const {
    std::ofstream f(path);
    if (!f) return false;
    f << "# Sideways City - safe to edit by hand\n";
    for (auto& kv : values) f << kv.first << " = " << kv.second << "\n";
    return true;
}

std::string Config::Get(const std::string& k, const std::string& def) const {
    auto it = values.find(k);
    return it == values.end() ? def : it->second;
}
float Config::GetFloat(const std::string& k, float def) const {
    auto it = values.find(k);
    return it == values.end() ? def : (float)std::atof(it->second.c_str());
}
int Config::GetInt(const std::string& k, int def) const {
    auto it = values.find(k);
    return it == values.end() ? def : std::atoi(it->second.c_str());
}
bool Config::GetBool(const std::string& k, bool def) const {
    auto it = values.find(k);
    return it == values.end() ? def : (it->second == "1" || it->second == "true");
}
void Config::SetFloat(const std::string& k, float v) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%g", v);
    values[k] = buf;
}

void Config::ApplyArgs(int argc, char** argv) {
    for (int i = 1; i + 1 < argc; i++) {
        if (argv[i][0] == '+') { values[argv[i] + 1] = argv[i + 1]; i++; }
    }
}

void SetConfigDefaults(Config& c) {
    c.values = {
        {"width", "1280"}, {"height", "720"},
        {"fullscreen", "0"}, {"vsync", "1"}, {"fps_max", "144"},
        {"msaa", "1"},
    };
}
