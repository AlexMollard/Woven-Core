# C++ Coding Style Guide

Some of these will be automatically enforced by the `.clang-format` file. The rest are just guidelines to follow for consistency.

## 1. Formatting

### 1.1 Indentation
- Use **tabs** for indentation.
- Keep indentation consistent within a file (don’t mix tabs/spaces).

### 1.2 Braces (Allman)
- Use **Allman** style braces: opening brace on the next line for:
  - classes/structs
  - functions/methods
  - namespaces
  - control blocks (`if/else`, loops, `switch`, etc.)

```cpp
class Thing
{
public:
	void DoWork()
	{
		if (condition)
		{
			// ...
		}
	}
};
```

### 1.3 `#pragma once`
- Prefer `#pragma once` for include guards.

```cpp
#pragma once
```

### 1.4 Preprocessor directives
- Platform blocks are acceptable and may indent includes inside blocks.

```cpp
#if PLATFORM_WIN32
#	include "Win32NativeWindow.h"
#endif
```

---

## 2. Naming Conventions

### 2.1 Types
Use **PascalCase** for:
- classes
- structs
- type aliases (if you add them)
- enums

Examples:
- `Input`
- `Texture`
- `Quad`

### 2.2 Functions / Methods
Use **PascalCase** for function and method names.

Examples:
- `Init()`
- `Update()`
- `GetInstance()`
- `SetViewportRect(...)`

### 2.3 Variables
#### Local variables + parameters
Use **lowerCamelCase** for:
- function parameters
- local variables

Examples:
- `deltaTime`
- `pointOne`
- `pointTwo`
- `time`

#### Member variables
Use `m_` prefix + **PascalCase**:
- `m_Window`
- `m_Camera`
- `m_MousePos`
- `m_ViewportRect`

#### Static members
Prefer `s_` prefix + **PascalCase**:
- `s_Instance`
- `s_Device`
- `s_Context`

> Note: There is at least one static member in the repo that uses `m_` (`Window::m_SwapChain`). If you want strictness, standardize on `s_` for statics going forward.

### 2.4 Enums
- Use `enum class`.
- Enum type name is **PascalCase**.
- Enum values are **PascalCase**.

Example:
- `Filter::Nearest`, `Filter::Linear`
- `Wrap::ClampToEdge`

---

## 3. Class Layout

Recommended order (matches common usage in the repo):
1. `public:` interface first
2. `private:` implementation details last
3. Singleton/static accessors near the top when applicable

```cpp
class Example
{
public:
	static Example* GetInstance();

	void Init();
	void Update();

private:
	Example() = default;
	~Example() = default;

	static Example* s_Instance;

	int m_Value = 0;
};
```

---

## 4. Getters / Setters

### 4.1 Getters
- Prefer `GetX()` naming.
- Mark getters `const` when they don’t modify state.

```cpp
int GetWidth() const
{
	return m_Width;
}
```

### 4.2 Setters
- Prefer `SetX(...)` naming.

```cpp
void SetCamera(Camera* camera);
```

---

## 5. Const correctness / references

- Use `const` for methods that do not mutate object state: `Type GetX() const`.
- Use `const&` for passing larger objects like `std::string` where applicable.

```cpp
Texture(const std::string& path);
```

---

## 6. Includes

- It’s fine to group includes as:
  1. STL / external headers
  2. local headers

```cpp
#pragma once

#include <string>
#include <unordered_map>

#include "Keycode.h"
```

---

## 7. Comments

- Use simple `//` comments.
- Section dividers are acceptable.

```cpp
// --- Keyboard ---
// Returns true while the key is held down
bool GetKey(Keycode key);
```

---

## 8. Practical “Do / Don’t” Summary

### Do
- `class PlayerController { ... }`
- `void Update(float deltaTime);`
- `float m_ScrollY = 0.0f;`
- `static Input* s_Instance;`
- braces on new lines
- tabs for indentation

### Don’t
- `class player_controller { ... }` (no snake_case types)
- `void update(float DeltaTime);` (no lowerCamelCase method names)
- `float scroll_y;` (no snake_case members)
- K&R braces (`if (...) {`) when matching this style

---