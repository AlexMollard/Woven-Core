#pragma once

#include "pch.hpp"

#include <filesystem>
#include <vector>

namespace FileSystem
{
	std::filesystem::path GetBasePath();
	std::filesystem::path GetCurrentPath();
	std::filesystem::path FindProjectRoot();
	std::filesystem::path GetAssetsDir();
	std::filesystem::path GetShadersDir();
	std::filesystem::path GetFontPath(const std::string& fileName);
	std::vector<uint8_t> LoadFile(const std::filesystem::path& path);
} // namespace FileSystem
