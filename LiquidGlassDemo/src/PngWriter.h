#pragma once
#include <cstdint>
#include <string>

namespace png {

bool writeRgba(const std::wstring& path, int width, int height, const uint8_t* rgba);
} 
