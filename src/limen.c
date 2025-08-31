// limen entry: parse CLI and dispatch commands

#include "cli.h"
#include "cmd_run.h"

#include <stdio.h>

int main(int argc, char **argv) {
  cli_context ctx = {0};
  if (cli_parse(argc, argv, &ctx) < 0) {
    cli_print_usage(argv[0], NULL, 0);
    return -1;
  }

  // Register available commands. "run" is the default behavior.
  const struct cli_command commands[] = {
      {.name = "run", .run = cmd_run, .desc = "Attach XDP to an interface (default)"},
  };
  const size_t ncmds = sizeof(commands) / sizeof(commands[0]);

  if (ctx.help) {
    cli_print_usage(argv[0], commands, ncmds);
    return 0;
  }

  int rc = cli_dispatch(&ctx, commands, ncmds, "run");
  if (rc < 0) {
    // On error, print usage for convenience
    cli_print_usage(argv[0], commands, ncmds);
  }
  return rc;
}
