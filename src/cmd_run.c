#include "cmd_run.h"

#include "limen_entry.skel.h"
#include <errno.h>
#include <linux/if_link.h>
#include <net/if.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static const uint8_t RSS_KEY_40B[40] = {
    0x6d, 0x5a, 0x56, 0xda, 0x25, 0x34, 0x23, 0x4e, 0x35, 0x6c,
    0x5b, 0x5a, 0x6c, 0x7a, 0x25, 0x37, 0x3d, 0x4e, 0x5f, 0x7a,
    0x6d, 0x5a, 0x56, 0xda, 0x25, 0x34, 0x23, 0x4e, 0x35, 0x6c,
    0x5b, 0x5a, 0x6c, 0x7a, 0x25, 0x37, 0x3d, 0x4e, 0x5f, 0x7a};

static volatile sig_atomic_t exiting = 0;
static void handle_signal(int sig) {
  (void)sig;
  exiting = 1;
}

static inline uint32_t key32_at_bit(const uint8_t *key, int bit_off) {
  // 取从 bit_off 开始的 32 个比特（大端）：
  int byte = bit_off / 8;
  int sh = bit_off % 8;
  uint64_t w = ((uint64_t)key[byte] << 32) | ((uint64_t)key[byte + 1] << 24) |
               ((uint64_t)key[byte + 2] << 16) |
               ((uint64_t)key[byte + 3] << 8) | ((uint64_t)key[byte + 4]);
  return (uint32_t)((w << sh) >> 8);
}

void build_tbl(const uint8_t *rss_key, uint32_t tbl[32][256]) {
  // 假设 klen >= 5 且起始偏移为 0；典型 RSS key 为 40 字节
  for (int pos = 0; pos < 32; pos++) {
    for (int v = 0; v < 256; v++) {
      uint32_t acc = 0;
      int off = pos;
      for (int b = 0; b < 8; b++) {
        if (v & (1 << (7 - b))) {
          acc ^= key32_at_bit(rss_key, off);
        }
        off++;
      }
      tbl[pos][v] = acc;
    }
  }
}

int cmd_run(cli_context *ctx) {
  if (!ctx)
    return -1;

  // Device name is the first positional argument to this command
  if (ctx->argc < 1 || !ctx->argv[0]) {
    fprintf(stderr, "Device name required. Try --help.\n");
    return -1;
  }

  const char *ifname = ctx->argv[0];
  unsigned ifindex = if_nametoindex(ifname);
  if (ifindex == 0) {
    fprintf(stderr, "if_nametoindex(%s) failed: %d (%s)\n", ifname, errno,
            strerror(errno));
    return -1;
  }

  struct limen_entry_bpf *skel = limen_entry_bpf__open();
  if (!skel) {
    fprintf(stderr, "open skeleton failed\n");
    return -1;
  }

  uint32_t tbl[32][256];

  build_tbl(RSS_KEY_40B, tbl);

  memcpy((void *)skel->rodata->toeplitz_tbl, tbl, sizeof(tbl));

  // 再 load
  if (limen_entry_bpf__load(skel)) {
    fprintf(stderr, "load skeleton failed\n");
    limen_entry_bpf__destroy(skel);
    return -1;
  }

  struct bpf_link *link =
      bpf_program__attach_xdp(skel->progs.xdp_prog, ifindex);
  long err = libbpf_get_error(link);
  if (err) {
    fprintf(stderr, "attach XDP failed on ifindex %u: %ld (%s)\n", ifindex, err,
            strerror(-err));
    limen_entry_bpf__destroy(skel);
    return -1;
  }
  skel->links.xdp_prog = link;

  printf("Successfully started! Press Ctrl-C to stop.\n");

  struct sigaction sa = {0};
  sa.sa_handler = handle_signal;
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);

  while (!exiting) {
    pause();
  }
  printf("\nExiting...\n");

  limen_entry_bpf__destroy(skel);
  return 0;
}
