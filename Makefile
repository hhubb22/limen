## Top-level Makefile: orchestrates subdir builds with a shared build dir

.PHONY: all bpf skeletons user clean help

# Root-level build directory
BUILD_DIR ?= $(CURDIR)/build

# User-space build (uses generated *.skel.h from build/)
USER_SRCS ?= src/limen.c src/cli.c src/cmd_run.c
USER_BIN  ?= $(BUILD_DIR)/limen
USER_CFLAGS ?= -O2 -g -Wall -Wextra

# Include generated skeletons and libbpf headers
CPPFLAGS += -I$(BUILD_DIR) $(shell pkg-config --cflags libbpf 2>/dev/null)
LDLIBS   += $(shell pkg-config --libs libbpf 2>/dev/null || echo -lbpf -lelf -lz)

# Infer BPF sources and corresponding skeleton headers
BPF_SRCS  := $(wildcard src/*.bpf.c)
BPF_SKELS := $(patsubst src/%.bpf.c,$(BUILD_DIR)/%.skel.h,$(BPF_SRCS))

all: user

# Build eBPF objects (delegates to src)
bpf:
	$(MAKE) -C src BUILD_DIR=$(BUILD_DIR)

# Optionally generate libbpf skeleton headers
skeletons:
	$(MAKE) -C src skeletons BUILD_DIR=$(BUILD_DIR)

# Ensure build dir exists
$(BUILD_DIR):
	@mkdir -p $@

# Build user-space binary, after skeletons are generated
user: $(USER_BIN)

$(USER_BIN): $(USER_SRCS) $(BPF_SKELS) | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(USER_CFLAGS) $(USER_SRCS) -o $@ $(LDLIBS)

# Regenerate a specific skeleton when its source changes
$(BUILD_DIR)/%.skel.h: src/%.bpf.c | $(BUILD_DIR)
	$(MAKE) -C src $@ BUILD_DIR=$(BUILD_DIR)

clean:
	$(MAKE) -C src clean BUILD_DIR=$(BUILD_DIR)

help:
	@echo "Targets:"
	@echo "  user        - build user-space binary $(USER_BIN)"
	@echo "  bpf         - build eBPF *.bpf.o into $(BUILD_DIR)/"
	@echo "  skeletons   - generate libbpf *.skel.h into $(BUILD_DIR)/"
	@echo "  clean       - remove build artifacts"
	@echo "Notes: BUILD_DIR can be overridden, default: $(BUILD_DIR)"
