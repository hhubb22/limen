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

static volatile sig_atomic_t exiting = 0;
static void handle_signal(int sig) {
  (void)sig;
  exiting = 1;
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

  struct limen_entry_bpf *skel = limen_entry_bpf__open_and_load();
  if (!skel) {
    fprintf(stderr, "Failed to open and load BPF skeleton\n");
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
