#include "pch.hpp"

// Override global new/delete to report allocations to Tracy with full callstacks
// Using TracyAllocS for explicit callstack depth control

void* operator new(std::size_t size)
{
	void* ptr = malloc(size);
	if (!ptr)
		throw std::bad_alloc();
	TracyAllocS(ptr, size, 10); // Capture 10 levels of callstack
	return ptr;
}

void* operator new[](std::size_t size)
{
	void* ptr = malloc(size);
	if (!ptr)
		throw std::bad_alloc();
	TracyAllocS(ptr, size, 10);
	return ptr;
}

void* operator new(std::size_t size, const std::nothrow_t&) noexcept
{
	void* ptr = malloc(size);
	if (ptr)
		TracyAllocS(ptr, size, 10);
	return ptr;
}

void* operator new[](std::size_t size, const std::nothrow_t&) noexcept
{
	void* ptr = malloc(size);
	if (ptr)
		TracyAllocS(ptr, size, 10);
	return ptr;
}

void operator delete(void* ptr) noexcept
{
	TracyFreeS(ptr, 10); // Match alloc depth
	free(ptr);
}

void operator delete[](void* ptr) noexcept
{
	TracyFreeS(ptr, 10);
	free(ptr);
}

void operator delete(void* ptr, std::size_t size) noexcept
{
	TracyFreeS(ptr, 10);
	free(ptr);
}

void operator delete[](void* ptr, std::size_t size) noexcept
{
	TracyFreeS(ptr, 10);
	free(ptr);
}
