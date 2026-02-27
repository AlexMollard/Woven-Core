#include "pch.hpp"

#include <SDL3/SDL.h>

#include "core/FileSystem.hpp"
#include "core/Logger.hpp"

namespace
{
	std::filesystem::path NormalizePath(const std::filesystem::path& path)
	{
		std::error_code ec;
		return std::filesystem::weakly_canonical(path, ec);
	}

	bool HasAssetsDir(const std::filesystem::path& root)
	{
		return std::filesystem::exists(root / "assets");
	}

	bool HasShadersDir(const std::filesystem::path& root)
	{
		return std::filesystem::exists(root / "shaders");
	}
} // namespace

namespace FileSystem
{
	std::filesystem::path GetBasePath()
	{
		const char* base = SDL_GetBasePath();
		if (!base)
		{
			return {};
		}
		std::filesystem::path result(base);
		return NormalizePath(result);
	}

	std::filesystem::path GetCurrentPath()
	{
		std::error_code ec;
		return NormalizePath(std::filesystem::current_path(ec));
	}

	std::filesystem::path FindProjectRoot()
	{
		std::vector<std::filesystem::path> candidates;
		const std::filesystem::path base = GetBasePath();
		const std::filesystem::path cwd = GetCurrentPath();

		if (!base.empty())
		{
			candidates.push_back(base);
			candidates.push_back(base.parent_path());
			candidates.push_back(base.parent_path().parent_path());
		}

		if (!cwd.empty())
		{
			candidates.push_back(cwd);
			candidates.push_back(cwd.parent_path());
			candidates.push_back(cwd.parent_path().parent_path());
		}

		for (const auto& candidate: candidates)
		{
			if (candidate.empty())
			{
				continue;
			}
			if (HasAssetsDir(candidate) || HasShadersDir(candidate))
			{
				return NormalizePath(candidate);
			}
		}

		return cwd;
	}

	std::filesystem::path GetAssetsDir()
	{
		const std::filesystem::path root = FindProjectRoot();
		return root / "assets";
	}

	std::filesystem::path GetShadersDir()
	{
		const std::filesystem::path root = FindProjectRoot();
		return root / "shaders";
	}

	std::filesystem::path GetFontPath(const std::string& fileName)
	{
		return GetAssetsDir() / "fonts" / fileName;
	}

	std::vector<uint8_t> LoadFile(const std::filesystem::path& path)
	{
		std::vector<uint8_t> data;
		if (path.empty())
		{
			return data;
		}

		size_t size = 0;
		void* buffer = SDL_LoadFile(path.string().c_str(), &size);
		if (!buffer || size == 0)
		{
			if (buffer)
			{
				SDL_free(buffer);
			}
			return data;
		}

		data.resize(size);
		std::memcpy(data.data(), buffer, size);
		SDL_free(buffer);
		return data;
	}
} // namespace FileSystem
