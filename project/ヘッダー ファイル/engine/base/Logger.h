#pragma once

#include <filesystem>
#include <d3d12.h>
#include "StringUtility.h"
#include <ostream>

using namespace StringUtility;

namespace Logger {
    void Log(const std::string& message);
    void Log(const std::wstring& message);
    void Log(std::ostream& os, const std::string& message);
}
