#include "Logger.hpp"

#include <cstdio>
#include <cstring>

#include "pch.hpp"

#ifdef _WIN32
#	include <Windows.h>
#endif

// ANSI color codes (compile-time constants)
namespace Color
{
	constexpr const char* Reset = "\033[0m";
	constexpr const char* Gray = "\033[90m";
	constexpr const char* Cyan = "\033[96m";
	constexpr const char* Yellow = "\033[93m";
	constexpr const char* Red = "\033[91m";
	constexpr const char* Blue = "\033[94m";
} // namespace Color

// Fast string matching helpers
static inline bool contains(const char* str, const char* substr)
{
	return strstr(str, substr) != nullptr;
}

void Logger::Init()
{
	// Enable ANSI color codes on Windows
#ifdef _WIN32
	// Enable VT100 processing for Windows Terminal
	HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
	if (hOut != INVALID_HANDLE_VALUE)
	{
		DWORD dwMode = 0;
		if (GetConsoleMode(hOut, &dwMode))
		{
			dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
			SetConsoleMode(hOut, dwMode);
		}
	}
#endif

	printf("%s=== Woven Core ===%s\n\n", Color::Cyan, Color::Reset);
	fflush(stdout);
}

void Logger::Shutdown()
{
	printf("\n%s=== Shutdown Complete ===%s\n", Color::Gray, Color::Reset);
}

void Logger::LogFormatted(LogLevel level, const char* format, va_list args)
{
	const char* color;
	const char* prefix;

	switch (level)
	{
		case LogLevel::Debug:
			color = Color::Gray;
			prefix = "[DEBUG]";
			break;
		case LogLevel::Info:
			color = Color::Cyan;
			prefix = "[INFO] ";
			break;
		case LogLevel::Warning:
			color = Color::Yellow;
			prefix = "[WARN] ";
			break;
		case LogLevel::Error:
			color = Color::Red;
			prefix = "[ERROR]";
			break;
	}

	printf("%s%s%s ", color, prefix, Color::Reset);
	vprintf(format, args);
	putchar('\n');
}

void Logger::Debug(const char* format, ...)
{
#ifndef NDEBUG
	va_list args;
	va_start(args, format);
	LogFormatted(LogLevel::Debug, format, args);
	va_end(args);
#endif
}

void Logger::Info(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	LogFormatted(LogLevel::Info, format, args);
	va_end(args);
}

void Logger::Warning(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	LogFormatted(LogLevel::Warning, format, args);
	va_end(args);
}

void Logger::Error(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	LogFormatted(LogLevel::Error, format, args);
	va_end(args);
}

void Logger::VulkanError(const char* message)
{
	printf("%s[ERROR] Vulkan%s\n  %s\n\n", Color::Red, Color::Reset, message);
}

void Logger::VulkanWarning(const char* message)
{
	// Fast filter: check first character for common patterns
	char first = message[0];

	// "DebugPrintf logs..." or "validation..." start with 'D' or 'v'
	if (first == 'v' && contains(message, "DebugPrintf"))
		return;

	if (first == 'v' && contains(message, "validation option was enabled"))
		return;

	if (first == 'v' && message[1] == 'k' && contains(message, "validation is adjusting settings"))
		return;

	printf("%s[WARN]  Vulkan%s\n  %s\n\n", Color::Yellow, Color::Reset, message);
}

void Logger::VulkanInfo(const char* message)
{
	// Only show shader debugPrintfEXT() output
	// Fast check: DEBUG-PRINTF starts with 'D'
	if (message[0] == 'D' && contains(message, "DEBUG-PRINTF"))
	{
		printf("%s[INFO]  Vulkan DebugPrintf%s\n  %s\n\n", Color::Blue, Color::Reset, message);
	}
	// Suppress all loader spam
}
