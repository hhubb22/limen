# Repository Guidelines

## Project Structure & Module Organization
- `src/`: user-space entry `limen.c`, eBPF programs `*.bpf.c`, CO-RE `vmlinux.h`, and `src/Makefile`.
- `build/`: compiled artifacts (`build/limen`, `*.bpf.o`, `*.skel.h`). Created by `make`; not committed.
- `scripts/`: local testing utilities (e.g., `scripts/netns-veth.sh`).
- Top-level: `Makefile` orchestrates builds; `justfile` wraps common tasks.

## Build, Test, and Development Commands
- **Dependencies**: `clang/llvm`, `bpftool`, `libbpf` (via `pkg-config`), `make`, `sudo`; optional `bear`.
- **Build all**: `make` → `build/limen` and BPF objects.
- **BPF only**: `make bpf`; **skeletons**: `make skeletons`; **user binary**: `make user`.
- **Run**: `just run <ifname>` or `sudo ./build/limen <ifname>`; detach: `sudo ./build/limen --detach <ifname>`.
- **Compile DB**: `just bear` to refresh `compile_commands.json`.
- **Test netns/veth**: `./scripts/netns-veth.sh create|status|delete`.
- **Alternate out dir**: `BUILD_DIR=/tmp/out make`.
- **CO-RE header**: `make -C src vmlinux.h` when targeting a different kernel.

## Coding Style & Naming Conventions
- **Language**: C (user + eBPF). Prefer C11 features verifier-safe in BPF code.
- **Indentation**: 2 spaces, no tabs; 100‑column soft limit.
- **Filenames**: eBPF sources `name.bpf.c`; generated headers `build/name.skel.h`; user entry `limen.c`.
- **Includes**: keep consistent; place local skeleton includes first.
- **Errors**: return negative on failure; log with `fprintf(stderr, ...)`.

## Testing Guidelines
- No unit framework yet; use the netns harness:
  - Create: `./scripts/netns-veth.sh create`
  - Attach (host): `sudo ./build/limen limen0`
  - Verify: `bpftool prog show` or `ip -details link show limen0`
  - Ping: `ip netns exec limen-test ping -c1 10.200.1.1`
  - Detach/Clean: `sudo ./build/limen --detach limen0`; `./scripts/netns-veth.sh delete`
- Prefer end‑to‑end PASS/DROP checks before proposing behavioral changes.

## Commit & Pull Request Guidelines
- **Commits**: imperative mood with scoped prefixes (e.g., `bpf: add xdp pass`, `build: make skeletons optional`).
- **PRs**: include description, reproduction steps, interface used (`limen0`, `eth0`), expected vs. actual behavior, and environment (kernel, arch). Do not commit artifacts: `build/`, `vmlinux.h`, `compile_commands.json`.

## Security & Configuration Tips
- Root is required to attach XDP and manage namespaces; commands self‑escalate via `sudo` where applicable.
- CO‑RE builds depend on `vmlinux.h`. Regenerate when targeting a different kernel.

