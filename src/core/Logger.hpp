#pragma once

#include <cstdarg>

enum class LogLevel
{
	Debug,
	Info,
	Warning,
	Error
};

// Format checking attributes (compiler-specific)
#if defined(__GNUC__) || defined(__clang__)
#	define LOGGER_PRINTF_FORMAT(fmt_idx, arg_idx) __attribute__((format(printf, fmt_idx, arg_idx)))
#else
#	define LOGGER_PRINTF_FORMAT(fmt_idx, arg_idx)
#endif

class Logger
{
public:
	static void Init();
	static void Shutdown();

	// Variadic logging (handles both plain strings and formatted output)
	static void Debug(const char* format, ...) LOGGER_PRINTF_FORMAT(1, 2);
	static void Info(const char* format, ...) LOGGER_PRINTF_FORMAT(1, 2);
	static void Warning(const char* format, ...) LOGGER_PRINTF_FORMAT(1, 2);
	static void Error(const char* format, ...) LOGGER_PRINTF_FORMAT(1, 2);

	// Vulkan-specific
	static void VulkanError(const char* message);
	static void VulkanWarning(const char* message);
	static void VulkanInfo(const char* message);

private:
	static void LogFormatted(LogLevel level, const char* format, va_list args);
};
