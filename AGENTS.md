# Repository Guidelines

## Coding Style & Naming Conventions

### File and Directory Naming

- Use lowercase `snake_case` for project-owned source files and directories.
- Examples: `vector.cpp`, `graphics_engine.h`, `render_pass/`.
- Name unit test files `<subject>_test.cpp`.
- Place SDK math tests in `MyDXEngineSDK/tests/math/`.
- Examples: `vector2_test.cpp`, `vector3_test.cpp`, `matrix_test.cpp`.
- Preserve tool-required filenames such as `CMakeLists.txt` and `AGENTS.md`.
- Preserve third-party file and directory names.
- Apply this convention to new files and directories. Do not rename existing
  files or directories unless explicitly requested.
- This rule governs file and directory names only; it does not change naming
  conventions for C++ types, functions, or variables.

### Initialization

This project follows a **fail-fast initialization policy**.

All required objects, components, and dependencies are validated **once at startup**.
If any required reference is missing, the application must **throw an explicit error**.

After successful initialization, the codebase **assumes all required objects exist**.
Repeated null / existence checks are intentionally avoided.

Example:

```csharp
public void Awake()
{
        if (player == null)
            throw new MissingReferenceException(
                $"{nameof(Example)}: Player is required but not assigned.");

        if (gameManager == null)
            throw new MissingReferenceException(
                $"{nameof(Example)}: GameManager is required but not assigned.");
}
```

When pointing out issues in code, always include line numbers (e.g., src/main.py:173).

在程式裡產生註解時，使用繁體中文，編碼為 UTF-8

No matter what language I use, always respond in English.

When generating comments in the code, use Traditional Chinese and UTF-8 encoding.

## Matrix Convention

The renderer uses one explicit matrix convention across C++ and HLSL:

- CPU matrices use the DirectXMath row-major, row-vector convention.
- Every HLSL matrix declaration must explicitly use `row_major`.
- HLSL matrix multiplication must use `mul(vector, matrix)`.
- Upload CPU matrices directly; do not transpose matrices merely to adapt their memory layout.
- Mathematical transposes, such as an inverse-transpose normal matrix, remain valid and must be named to describe their mathematical purpose.
- When changing a matrix constant buffer, update and verify its matching C++ structure and HLSL declaration together.

Avoid unnecessary defensive existence/null checks when the invariant is already established by the current control flow.
- Keep checks for factories, async state, user input, and teardown-sensitive paths.
- For internal invariants already proven in the same flow, prefer direct code; use assertions if clarification is needed.

## Handling old functions after refactoring
When refactoring code, do not immediately delete code that appear unused.

Default rule: 
- Mark them with [[deprecated("...")]] first. 
- Explain whether they became unused because of refactoring, API replacement, or architecture changes. 
- If there is a replacement code, include it in the deprecation message.

Example:
cpp
[[deprecated("Unused after PB tree tracking refactor. Use BuildPbTreeFromReaderState() instead.")]]
void BuildLegacyPbTree();
