#!/usr/bin/env bash

# Create and manage a veth pair across default and a new netns.
#
# Best practices:
# - Strict mode, clear logging, idempotent operations
# - Self-escalate to root via sudo, or exit with guidance
# - Parameters via flags with sane defaults
# - Separate create/delete/status subcommands
#
# Usage examples:
#   scripts/netns-veth.sh create                 # use defaults
#   scripts/netns-veth.sh create -n limen-ns \
#       -l limen0 -r limen1 -L 10.200.1.1/24 -R 10.200.1.2/24 --mtu 1500
#   scripts/netns-veth.sh status                 # print current state
#   scripts/netns-veth.sh delete                 # teardown veth + namespace

set -Eeuo pipefail

log() { echo "[netns-veth] $*"; }
err() { echo "[netns-veth][error] $*" >&2; }

need_cmd() {
  command -v "$1" >/dev/null 2>&1 || { err "Missing dependency: $1"; exit 127; }
}

# Re-exec as root if needed
if [[ ${EUID:-$(id -u)} -ne 0 ]]; then
  if command -v sudo >/dev/null 2>&1; then
    exec sudo -E "$0" "$@"
  else
    err "This script requires root. Install sudo or run as root."
    exit 1
  fi
fi

need_cmd ip

# Defaults (can be overridden with flags)
NS_NAME=${NS_NAME:-limen-test}
HOST_IF=${HOST_IF:-limen0}
NS_IF=${NS_IF:-limen1}
HOST_ADDR=${HOST_ADDR:-10.200.1.1/24}
NS_ADDR=${NS_ADDR:-10.200.1.2/24}
MTU=${MTU:-1500}

usage() {
  cat <<EOF
Usage: $0 <create|delete|status> [options]

Options:
  -n, --ns NAME           Netns name (default: ${NS_NAME})
  -l, --left IF           Host-side veth name (default: ${HOST_IF})
  -r, --right IF          Netns-side veth name (default: ${NS_IF})
  -L, --left-addr CIDR    Host-side IPv4 addr (default: ${HOST_ADDR})
  -R, --right-addr CIDR   Netns-side IPv4 addr (default: ${NS_ADDR})
      --mtu MTU           Interface MTU (default: ${MTU})
  -h, --help              Show this help

Environment overrides also supported: NS_NAME, HOST_IF, NS_IF, HOST_ADDR, NS_ADDR, MTU
EOF
}

parse_args() {
  if [[ $# -lt 1 ]]; then
    usage; exit 2
  fi
  ACTION=$1; shift || true

  while [[ $# -gt 0 ]]; do
    case "$1" in
      -n|--ns) NS_NAME=$2; shift 2 ;;
      -l|--left) HOST_IF=$2; shift 2 ;;
      -r|--right) NS_IF=$2; shift 2 ;;
      -L|--left-addr) HOST_ADDR=$2; shift 2 ;;
      -R|--right-addr) NS_ADDR=$2; shift 2 ;;
      --mtu) MTU=$2; shift 2 ;;
      -h|--help) usage; exit 0 ;;
      *) err "Unknown option: $1"; usage; exit 2 ;;
    esac
  done
}

ns_exists() { ip netns list | awk '{print $1}' | grep -Fxq "$NS_NAME"; }
link_exists() { ip link show "$1" >/dev/null 2>&1; }
ns_link_exists() { ip -n "$NS_NAME" link show "$1" >/dev/null 2>&1; }

ensure_ns() {
  if ns_exists; then
    log "Netns '$NS_NAME' already exists"
  else
    log "Creating netns '$NS_NAME'"
    ip netns add "$NS_NAME"
  fi
  # Ensure loopback is up
  ip -n "$NS_NAME" link set lo up || true
}

ensure_veth() {
  if link_exists "$HOST_IF"; then
    log "Host veth '$HOST_IF' already exists"
  else
    log "Creating veth pair '$HOST_IF' <-> '$NS_IF'"
    ip link add "$HOST_IF" type veth peer name "$NS_IF"
  fi

  # Move NS side into namespace if not already there
  if ns_link_exists "$NS_IF"; then
    : # already in ns
  else
    # If the peer still resides in default ns, move it
    if link_exists "$NS_IF"; then
      ip link set "$NS_IF" netns "$NS_NAME"
    else
      # Could happen if the pair was created previously and host side persisted
      # Try to detect peer inside ns regardless
      if ! ns_link_exists "$NS_IF"; then
        err "Peer interface '$NS_IF' not found in any namespace"
        exit 1
      fi
    fi
  fi

  # MTU and admin up
  ip link set "$HOST_IF" mtu "$MTU" up
  ip -n "$NS_NAME" link set "$NS_IF" mtu "$MTU" up

  # Assign addresses idempotently (ignore if already added)
  ip addr add "$HOST_ADDR" dev "$HOST_IF" 2>/dev/null || true
  ip -n "$NS_NAME" addr add "$NS_ADDR" dev "$NS_IF" 2>/dev/null || true
}

create() {
  ensure_ns
  ensure_veth
  log "Created veth pair: ${HOST_IF} <-> ${NS_IF} in netns '${NS_NAME}'"
  log "Host ${HOST_IF} addr: ${HOST_ADDR}, MTU ${MTU}"
  log "NS   ${NS_IF} addr: ${NS_ADDR}, MTU ${MTU}"
  log "Try: ping -c1 ${NS_ADDR%%/*} (from host) or ip netns exec ${NS_NAME} ping -c1 ${HOST_ADDR%%/*}"
}

delete() {
  # Deleting host side removes peer automatically
  if link_exists "$HOST_IF"; then
    log "Deleting host link '$HOST_IF' (and peer)"
    ip link del "$HOST_IF"
  else
    log "Host link '$HOST_IF' not present; skipping"
  fi

  if ns_exists; then
    log "Deleting netns '$NS_NAME'"
    ip netns del "$NS_NAME"
  else
    log "Netns '$NS_NAME' not present; skipping"
  fi
}

status() {
  echo "--- netns list ---"
  ip netns list || true
  echo
  echo "--- host link (${HOST_IF}) ---"
  ip -details link show "$HOST_IF" || echo "(absent)"
  echo
  echo "--- ns link (${NS_NAME}:${NS_IF}) ---"
  ip -n "$NS_NAME" -details link show "$NS_IF" 2>/dev/null || echo "(absent)"
  echo
  echo "--- addresses ---"
  ip -o -4 addr show dev "$HOST_IF" 2>/dev/null || true
  ip -n "$NS_NAME" -o -4 addr show dev "$NS_IF" 2>/dev/null || true
}

main() {
  parse_args "$@"
  case "${ACTION}" in
    create) create ;;
    delete) delete ;;
    status) status ;;
    *) err "Unknown action: ${ACTION}"; usage; exit 2 ;;
  esac
}

main "$@"

