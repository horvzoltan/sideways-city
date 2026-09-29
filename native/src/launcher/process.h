#pragma once
#include <string>
// Starts `exe` as a detached process. Kept in its own file because windows.h
// clashes with raylib names.
bool LaunchDetached(const std::string& exe, std::string* error);
