# Building Banana Bay

Banana Bay is written in standard C (`-std=c99`) using **SDL2** and `clang` with zero external asset files.

The version number is read from [VERSION](VERSION) and embedded at compile time (`BANANA_BAY_VERSION`).

---

## Prerequisites (Linux)

### Native Build (`x86_64`)

On Debian / Ubuntu:

```bash
sudo apt update
sudo apt install clang make pkg-config libsdl2-dev
```

### ARM64 (`aarch64`) Cross-Compilation

To cross-compile the ARM64 release binary from an `x86_64` Debian / Ubuntu host:

```bash
sudo dpkg --add-architecture arm64
sudo apt update
sudo apt install lld gcc-aarch64-linux-gnu libsdl2-dev:arm64
```

> **Note:** `clang --target=aarch64-linux-gnu` uses `lld` and the `aarch64-linux-gnu` sysroot/libraries, statically linking `/usr/lib/aarch64-linux-gnu/libSDL2.a`.

---

## Building & Running

```bash
# Build native debug/dev binary (./banana-bay)
make

# Build and run
make run

# Print version and exit
./banana-bay --version

# Clean build artifacts (keeps releases/)
make clean
```

### Headless Smoke Test

```bash
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ./banana-bay
```

---

## Release Builds

Release binaries are placed in `releases/` and statically link `libSDL2.a` so end users do not need `libSDL2-2.0.so` installed (system glibc, audio, and X11/Wayland runtime libraries remain dynamically linked):

```bash
# Build all release targets (x86_64 + arm64)
make release

# Or build a single target
make release-x86
make release-arm64
```

Outputs (using [VERSION](VERSION)):
* `releases/banana-bay-linux-x86-<VERSION>`
* `releases/banana-bay-linux-arm64-<VERSION>`

---

## Planned Platforms (Roadmap)

The codebase is structured to keep platform-specific code isolated to [src/main.c](src/main.c) and [Makefile](Makefile) for future targets:

* **macOS (OS X):** `clang` + `sdl2` (via Homebrew or `SDL2.framework`), targeting universal (`x86_64` + `arm64`) binaries.
* **Web (WebAssembly):** Emscripten (`emcc -s USE_SDL=2`) with an `emscripten_set_main_loop()` step wrapper in `src/main.c`.
* **Windows:** `clang` / MinGW-w64 (`x86_64-w64-mingw32`) or MSVC + SDL2.
* **Android & iOS:** SDL2 mobile project wrappers (`SDL_main` entry point, touch/on-screen controls or gamepad input).
