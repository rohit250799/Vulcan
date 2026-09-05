# Build & Debug Guide

Canonical reference for building, testing, and debugging Vulcan. This
replaces the old Makefile-based workflow entirely — see [Migration History](#migration-history)
for where the old build went.

---

## 1. Build system overview

Vulcan uses **CMake** for the build graph (compiling, linking, dependency
tracking, per-target flags) and **[`just`](https://github.com/casey/just)**
as a thin command-shortcut layer on top — `just` has no build logic of its
own; every recipe just delegates to `cmake`/`ctest`/`perf`/`gdb`/`valgrind`.
If `just` were removed, every command below still works run by hand — see
each recipe's underlying command with `just --show <recipe>`.

### Presets

| Preset | Flags | Use for |
|---|---|---|
| `debug` | `-O0 -g3 -gdwarf-4 -ggdb -fno-omit-frame-pointer -fno-inline` | Everyday development, gdb, valgrind, sanitizers |
| `release` | `-O3 -DNDEBUG`, LTO enabled, symbols stripped | What actually ships / gets benchmarked end-to-end |
| `benchmark` | `-O3 -g -DNDEBUG -fno-omit-frame-pointer`, `-march=native` | Local profiling with `perf` (needs symbols; `-march=native` is **not portable** across machines) |

Reconfigure (`cmake --preset <name>`) whenever you add a new source file or
change a `CMakeLists.txt` option. Rebuild (`cmake --build --preset <name>`)
after any ordinary code edit — Ninja only recompiles what changed.

---

## 2. `just` command reference

Run `just --list` for the live list. Most recipes take an optional `preset`
argument (defaults shown).

| Command | Default preset | Does |
|---|---|---|
| `just build [preset]` | release | Configure + build |
| `just debug` / `just release` / `just bench-build` | — | Shortcuts for the three presets |
| `just run [preset]` | release | Build, then run the main binary (`sudo` — raw sockets) |
| `just bench` | benchmark | Build, then run the benchmark |
| `just test [preset]` | debug | Build, then run all tests via CTest |
| `just test-one <name> [preset]` | debug | Run one test by name |
| `just find-error [preset]` | debug | `gdb -batch -ex run -ex bt -ex quit` on the main binary |
| `just find-bench-error [preset]` | debug | Same, on the benchmark binary |
| `just valgrind [preset]` | debug | Full leak check, main binary |
| `just valgrind-bench [preset]` | debug | Full leak check, benchmark binary |
| `just helgrind [preset]` | debug | Data-race detection (see caveat in §3) |
| `just analyze-benchmark` | benchmark | Full `perf stat` event set → CSV in `benchmarks/results/` |
| `just check-latency [preset]` | release | `perf mem record` + report |
| `just check-false-sharing [preset]` | release | `perf c2c` — verifies cache-line padding is actually preventing false sharing |
| `just machine [preset]` | debug | Disassemble `main.cpp.o` |
| `just info [preset]` | release | Dump all CMake cache variables |
| `just format` | — | `clang-format -i` over the whole tree |
| `just clean` | — | Remove all build output |
| `just clean-preset <preset>` | — | Remove one preset's build output only |

---

## 3. Debugging toolkit

### gdb

```bash
gdb build/debug/src/vulcan           # needs a debug build for symbols
(gdb) run
(gdb) backtrace                      # after a crash
(gdb) thread apply all bt            # all threads — check this for producer/consumer bugs
(gdb) break file.cpp:123 if x == 1   # conditional breakpoint
```
Core dumps: `ulimit -c unlimited` first, then `gdb build/debug/src/vulcan core`.

### Valgrind

```bash
just valgrind             # leak check
just helgrind             # data races
```
**Caveat:** Valgrind emulates the CPU and doesn't fully understand real
hardware memory ordering — for the lock-free SPSC queue specifically, treat
Helgrind output as a lead to confirm with TSan, not a verdict on its own.

### Sanitizers (not wired up yet — add if/when needed)

```cmake
option(VULCAN_SANITIZE "" "")
if(VULCAN_SANITIZE)
  add_compile_options(-fsanitize=${VULCAN_SANITIZE} -fno-omit-frame-pointer)
  add_link_options(-fsanitize=${VULCAN_SANITIZE})
endif()
```
| Sanitizer | Catches | Flag |
|---|---|---|
| ASan | buffer overflows, use-after-free | `-DVULCAN_SANITIZE=address` |
| UBSan | signed overflow, misaligned access | `-DVULCAN_SANITIZE=undefined` |
| TSan | **data races** — highest value for the SPSC queue | `-DVULCAN_SANITIZE=thread` (not combinable with ASan) |

### perf

```bash
just analyze-benchmark          # full event-set stat, CSV output
just check-latency              # perf mem — memory access latency
just check-false-sharing        # perf c2c — false-sharing detection
perf top -p $(pgrep vulcan)     # live view while running
```

### objdump / addr2line

```bash
just machine                                          # disassemble main.cpp.o
addr2line -e build/debug/src/vulcan -f -C 0xADDRESS    # crash address -> file:line
objdump -d build/release/src/vulcan | grep vpermd      # confirm -march actually vectorized as expected
```

### strace

```bash
sudo strace -f -e trace=network build/release/src/vulcan   # relevant to raw_socket_udp_listener
sudo strace -c build/release/src/vulcan                     # syscall time summary
```

### Compiler Explorer (godbolt.org)

Use for isolated "does this actually vectorize / is this actually lock-free
at the instruction level" questions on a small self-contained snippet — not
for whole-program or LTO-dependent behavior. Match compiler (GCC 13) and
flags (`-O3 -march=znver3 -std=c++20`) to the real build for representative results.

---

## 4. Known project-specific caveats

- **Assertions are release no-ops.** `assert()` in `tests/` only fires under
  `debug` (no `-DNDEBUG`). A green `release` test run means "didn't crash,"
  not "assertions checked."
- **The SPSC queue spins on empty pop by design** — this is intentional for
  low-latency (avoids syscall/context-switch cost of blocking), not a bug.
  A prior race around this was fixed; if touching this code again, verify
  under `helgrind`/TSan before trusting it.
- **`std::hardware_destructive_interference_size` varies by compiler/`-march`.**
  Since presets switch between `znver3` and `native`, cache-line alignment in
  `lock_free_spsc_queue.hpp` could shift between builds. Consider pinning to
  an explicit `constexpr size_t CACHE_LINE_SIZE = 64;` if this hasn't been done.
- **`benchmark` preset uses `-march=native`** — the resulting binary is tied
  to the CPU it was built on and won't be portable.

---

## 5. Migration history

The project moved from a hand-written Makefile to CMake + `just`. The last
commit with the Makefile is tagged `pre-cmake-migration` — check it out if
you ever need to see exactly how the old build worked:

```bash
git show pre-cmake-migration:Makefile
```

Next planned step: incrementally converting `core/include` headers (fewest
dependents) to C++20 modules, starting only after this build has been stable
for a while.
