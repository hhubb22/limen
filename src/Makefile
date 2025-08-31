## Simple, best-practice Makefile for building eBPF objects (CO-RE)
## - Compiles all `*.bpf.c` in this directory to `build/*.bpf.o`
## - Generates `vmlinux.h` via bpftool if missing
## - Optional: generate libbpf skeleton headers

# Tools (override if needed)
CLANG       ?= clang
LLVM_STRIP  ?= llvm-strip
BPFTOOL     ?= bpftool

# Build output directory (root-level build next to src)
TOP         := $(abspath ..)
BUILD_DIR   ?= $(TOP)/build

# Host arch mapping to __TARGET_ARCH_*
UNAME_M     := $(shell uname -m)
ifeq ($(UNAME_M),x86_64)
TARGET_ARCH := x86
else ifeq ($(UNAME_M),aarch64)
TARGET_ARCH := arm64
else ifeq ($(UNAME_M),arm64)
TARGET_ARCH := arm64
else ifeq ($(UNAME_M),armv7l)
TARGET_ARCH := arm
else ifeq ($(UNAME_M),armv6l)
TARGET_ARCH := arm
else ifeq ($(UNAME_M),riscv64)
TARGET_ARCH := riscv64
else ifeq ($(UNAME_M),ppc64le)
TARGET_ARCH := powerpc
else
TARGET_ARCH := $(UNAME_M)
endif

# Common CFLAGS for BPF CO-RE builds
BPF_CFLAGS ?= \
  -g -O2 -Wall -Werror \
  -Wno-unused-value -Wno-pointer-sign -Wno-compare-distinct-pointer-types \
  -Wno-gnu-variable-sized-type-not-at-end -Wno-address-of-packed-member \
  -Wno-tautological-compare -fno-stack-protector -fno-asynchronous-unwind-tables \
  -fno-builtin -fno-unroll-loops \
  -D__TARGET_ARCH_$(TARGET_ARCH) -D__BPF_TRACING__ \
  -I. \
  -target bpf

# Sources and derived outputs
BPF_SRCS := $(wildcard *.bpf.c)
BPF_OBJS := $(patsubst %.bpf.c,$(BUILD_DIR)/%.bpf.o,$(BPF_SRCS))
SKELS    := $(patsubst $(BUILD_DIR)/%.bpf.o,$(BUILD_DIR)/%.skel.h,$(BPF_OBJS))

.PHONY: all clean help skeletons

all: $(BPF_OBJS)
	@echo "Built: $(BPF_OBJS)"

# Auto-generate CO-RE vmlinux.h if missing
vmlinux.h:
	@echo "Generating $@ from /sys/kernel/btf/vmlinux"
	$(BPFTOOL) btf dump file /sys/kernel/btf/vmlinux format c > $@

$(BUILD_DIR):
	@mkdir -p $@

# Compile .bpf.c -> build/.bpf.o (requires vmlinux.h in this dir)
$(BUILD_DIR)/%.bpf.o: %.bpf.c vmlinux.h | $(BUILD_DIR)
	$(CLANG) $(BPF_CFLAGS) -c $< -o $@
	@# Keep symbols for BTF and debugging; strip non-BTF debug info only
	@$(LLVM_STRIP) -g $@ >/dev/null 2>&1 || true

# Optional: generate libbpf skeleton headers next to objects
skeletons: $(SKELS)
	@echo "Skeletons: $(SKELS)"

$(BUILD_DIR)/%.skel.h: $(BUILD_DIR)/%.bpf.o
	$(BPFTOOL) gen skeleton $< > $@

clean:
	rm -rf $(BUILD_DIR)
	@# Keep vmlinux.h by default (delete it if you want regeneration)

help:
	@echo "Targets:"
	@echo "  all         - build all *.bpf.o into $(BUILD_DIR)/"
	@echo "  skeletons   - generate libbpf *.skel.h for built objects"
	@echo "  clean       - remove $(BUILD_DIR)/"
	@echo "Notes:"
	@echo "  - Edit BPF_CFLAGS as needed; defaults target to __TARGET_ARCH_$(TARGET_ARCH)."
	@echo "  - If libbpf headers are not installed, install libbpf-dev (or equivalent)."
