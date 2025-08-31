// Simple CLI parsing and dispatch helpers for limen

#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef struct cli_context {
  // Global flags
  bool help;      // --help / -h

  // Subcommand and remaining argv after global flags
  const char *subcommand; // NULL means default command
  int argc;                // remaining argc for command
  const char **argv;       // remaining argv for command
} cli_context;

// Command definition
typedef int (*cli_cmd_fn)(cli_context *ctx);
struct cli_command {
  const char *name;
  cli_cmd_fn run;
  const char *desc;
};

// Parse global flags and expose remaining args for command handling.
// Returns 0 on success, negative on failure (error already printed).
int cli_parse(int argc, char **argv, cli_context *out);

// Dispatch to a subcommand by name, falling back to default_cmd when
// ctx->subcommand is NULL or does not match any known command name.
// Returns what the command returns.
int cli_dispatch(cli_context *ctx, const struct cli_command *cmds, size_t ncmds,
                 const char *default_cmd);

// Print usage with available commands.
void cli_print_usage(const char *prog, const struct cli_command *cmds,
                     size_t ncmds);
