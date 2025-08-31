// Minimal CLI framework: parses global flags and exposes subcommand + args

#include "cli.h"

#include <getopt.h>
#include <stdio.h>
#include <string.h>

int cli_parse(int argc, char **argv, cli_context *out) {
  if (!out) return -1;
  memset(out, 0, sizeof(*out));

  // Supported global long options
  static const struct option long_opts[] = {
      {"help", no_argument, 0, 'h'},
      {0, 0, 0, 0},
  };

  // Parse global flags; allow permutation so flags can appear anywhere
  int opt;
  int longidx = 0;
  // opstring without '+' enables argv permutation under glibc
  while ((opt = getopt_long(argc, argv, "h", long_opts, &longidx)) != -1) {
    switch (opt) {
    case 'h':
      out->help = true;
      break;
    case '?':
    default:
      // Unknown option; getopt_long already printed an error
      return -1;
    }
  }

  // The first remaining non-option token is considered the subcommand.
  // If you want default command, leave it NULL and let dispatcher decide.
  if (optind < argc) {
    out->subcommand = argv[optind];
    out->argc = argc - optind - 1;
    out->argv = (const char **)&argv[optind + 1];
  } else {
    out->subcommand = NULL;
    out->argc = 0;
    out->argv = NULL;
  }

  return 0;
}

void cli_print_usage(const char *prog, const struct cli_command *cmds,
                     size_t ncmds) {
  fprintf(stderr,
          "Usage: %s [global options] [command] [args]\n\n"
          "Global options:\n"
          "  -h, --help     Show this help\n\n"
          "Default: 'run' when no command is provided\n\n"
          "Commands:\n",
          prog);
  for (size_t i = 0; i < ncmds; i++) {
    fprintf(stderr, "  %-10s %s\n", cmds[i].name, cmds[i].desc ? cmds[i].desc : "");
  }
  fprintf(stderr, "\nExamples:\n");
  fprintf(stderr, "  %s limen0             # attach to interface (default)\n", prog);
  fprintf(stderr, "  %s run limen0         # explicit run command\n", prog);
}

int cli_dispatch(cli_context *ctx, const struct cli_command *cmds, size_t ncmds,
                 const char *default_cmd) {
  if (!cmds || ncmds == 0) {
    fprintf(stderr, "No commands registered\n");
    return -1;
  }

  // Decide effective command name
  const char *name = ctx->subcommand;
  if (!name) name = default_cmd;

  // If subcommand is not recognized, we may treat it as a positional arg
  const struct cli_command *chosen = NULL;
  for (size_t i = 0; i < ncmds; i++) {
    if (name && strcmp(name, cmds[i].name) == 0) {
      chosen = &cmds[i];
      break;
    }
  }

  bool use_default_with_sub_as_arg = false;
  if (!chosen && ctx->subcommand && default_cmd) {
    use_default_with_sub_as_arg = true;
  }

  // If still not found, try default command
  if (!chosen && default_cmd) {
    for (size_t i = 0; i < ncmds; i++) {
      if (strcmp(default_cmd, cmds[i].name) == 0) {
        chosen = &cmds[i];
        break;
      }
    }
  }

  if (!chosen) {
    fprintf(stderr, "No suitable command found\n");
    return -1;
  }

  int rc = 0;
  if (use_default_with_sub_as_arg) {
    // Build a temporary context so we don't mutate caller state
    cli_context tmp = *ctx;

    int merged_argc = 1 + ctx->argc;
    const char *merged_argv[merged_argc];
    merged_argv[0] = ctx->subcommand;
    for (int i = 0; i < ctx->argc; i++)
      merged_argv[i + 1] = ctx->argv ? ctx->argv[i] : NULL;

    tmp.argc = merged_argc;
    tmp.argv = merged_argv;
    tmp.subcommand = NULL;

    rc = chosen->run(&tmp);
  } else {
    rc = chosen->run(ctx);
  }
  return rc;
}
