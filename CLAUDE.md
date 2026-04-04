# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**ncbind** is a C++ template library that provides native class binding for Kirikiri Z (krkrz) plugins. It allows C++ classes to be registered and used from TJS2 (the scripting language in Kirikiri) without manually writing argument marshaling wrappers.

This is a header-only template library (plus a small `.cpp` for plugin entry points). It is used by other krkrz plugins as a dependency, not as a standalone application.

## Build

Using gcc/MinGW (Makefile):
```bash
make          # builds testbind.dll
make test     # builds and runs test in krkrz
make clean
```

For MSVC, compile with `/EXPORT:V2Link /EXPORT:V2Unlink` linker flags. Static plugin builds use the `TVP_STATIC_PLUGIN` and `TVP_PLUGIN_NAME` defines.

## Architecture

- **`ncbind.hpp`** — Main template library. Contains all registration macros (`NCB_REGISTER_CLASS`, `NCB_ATTACH_CLASS`, etc.), type conversion infrastructure (`ncbTypeConvertor`), class info storage (`ncbClassInfo<T>`), and the auto-registration system (`ncbAutoRegister`). This is the single include needed by plugin code.
- **`ncb_invoke.hpp`** — Template machinery for calling arbitrary C++ methods with TJS2 variant arguments. Handles method trait extraction, argument unpacking, and result conversion. Included by `ncbind.hpp`.
- **`ncb_foreach.h`** — Preprocessor include-macro that expands macros for variable argument counts (up to `FOREACH_MAX`). Used to generate overloads for different parameter counts.
- **`ncbind.cpp`** — Plugin entry points (`V2Link`/`V2Unlink`) and static variable definitions. Every plugin using ncbind links this file. Handles both DLL and static plugin (`TVP_STATIC_PLUGIN`) builds.
- **`testbind.cpp`** — Comprehensive test/example showing all registration patterns (classes, methods, properties, proxies, bridges, subclasses, callbacks, type converters).

## Key Patterns

Registration uses macros that expand to static initializers collected by `ncbAutoRegister`:
```cpp
NCB_REGISTER_CLASS(MyClass) {
  Constructor<int, ttstr>(0);
  Method("doSomething", &Class::doSomething);
  Property("value", &Class::getValue, &Class::setValue);
}
```

`Class` is a typedef for the registered class available inside the macro body. Method dispatch types: `Proxy` (static function with instance as first arg), `Bridge<Functor>()` (delegate to another class), `RawCallback` (direct tTJSVariant access).

## Known Limitations

- No inheritance support between registered classes (no `instanceof` across class hierarchy)
- No default parameter values; argument count must match exactly
- Only one constructor per class
- Namespace-qualified classes must be typedef'd outside the namespace before registration
- Returning registered class instances by pointer triggers delete on invalidate; by reference does not
