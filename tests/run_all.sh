#!/bin/sh
# yar-c test suite orchestrator.
#
# Phases:
#   1. standalone (single process) server on TCP  -> C suite + PHP suite
#   2. standalone server on a unix domain socket  -> C suite
#   3. daemonised pre-fork server (4 workers)     -> C concurrent suite
#
# Usage: sh tests/run_all.sh [--no-php]
#
# Environment:
#   YAR_TEST_PORT   TCP port for the standalone server   (default 19871)
#   YAR_TEST_DPORT  TCP port for the pre-fork daemon     (default 19872)
#   YAR_TEST_SOCK   unix socket path                     (default /tmp/yar_test_<pid>.sock)
#   YAR_TEST_LOGDIR directory for server/test logs       (default tests/logs)

set -u

cd "$(dirname "$0")" || exit 2

PORT=${YAR_TEST_PORT:-19871}
DPORT=${YAR_TEST_DPORT:-19872}
SOCK=${YAR_TEST_SOCK:-/tmp/yar_test_$$.sock}
LOGDIR=${YAR_TEST_LOGDIR:-$(pwd)/logs}
SKIP_PHP=0

for arg in "$@"; do
	case "$arg" in
		--no-php) SKIP_PHP=1 ;;
		*) echo "usage: $0 [--no-php]" >&2; exit 2 ;;
	esac
done

if [ ! -x ./yar_test_server ] || [ ! -x ./yar_test_client ]; then
	echo "error: test binaries missing, run 'make -C tests' first" >&2
	exit 2
fi

mkdir -p "$LOGDIR"

overall=0
tcp_pid=
unix_pid=
daemon_pid_file="$LOGDIR/daemon.pid"

cleanup() {
	[ -n "$tcp_pid" ] && kill "$tcp_pid" 2>/dev/null
	[ -n "$unix_pid" ] && kill "$unix_pid" 2>/dev/null
	if [ -f "$daemon_pid_file" ]; then
		kill "$(cat "$daemon_pid_file")" 2>/dev/null
	fi
	wait 2>/dev/null
	rm -f "$SOCK"
}
trap cleanup EXIT
trap 'exit 1' INT TERM

step() {
	printf '\n===== %s =====\n' "$1"
}

# --- 1. standalone TCP server ------------------------------------------------
step "starting standalone TCP server on 127.0.0.1:$PORT"
./yar_test_server -S "127.0.0.1:$PORT" -X -l "$LOGDIR/tcp.log" &
tcp_pid=$!

if ! ./yar_test_client --uri "tcp://127.0.0.1:$PORT" --probe; then
	echo "FATAL: TCP server did not come up (see $LOGDIR/tcp.log)" >&2
	[ -f "$LOGDIR/tcp.log" ] && cat "$LOGDIR/tcp.log" >&2
	exit 1
fi

step "C suite (tcp)"
./yar_test_client --uri "tcp://127.0.0.1:$PORT" || overall=1

# --- 2. standalone unix socket server ----------------------------------------
step "starting standalone unix server on $SOCK"
./yar_test_server -S "$SOCK" -X -l "$LOGDIR/unix.log" &
unix_pid=$!

if ! ./yar_test_client --uri "$SOCK" --probe; then
	echo "FATAL: unix server did not come up (see $LOGDIR/unix.log)" >&2
	[ -f "$LOGDIR/unix.log" ] && cat "$LOGDIR/unix.log" >&2
	exit 1
fi

step "C suite (unix socket)"
./yar_test_client --uri "$SOCK" || overall=1

# --- 3. PHP interop -----------------------------------------------------------
if [ "$SKIP_PHP" = "1" ]; then
	step "PHP suite skipped (--no-php)"
elif ! command -v php >/dev/null 2>&1; then
	step "PHP suite skipped (no php binary found)"
elif ! php -r 'exit(extension_loaded("yar") && extension_loaded("msgpack") ? 0 : 1);' >/dev/null 2>&1; then
	step "PHP suite skipped (yar/msgpack extension not loaded)"
else
	step "PHP suite (tcp)"
	php_fail=0
	for t in php/0*.php; do
		printf 'TEST %-44s ' "$t"
		if YAR_TEST_URI="tcp://127.0.0.1:$PORT" php -d yar.packager=msgpack "$t" \
				>"$LOGDIR/$(basename "$t").out" 2>&1; then
			echo PASS
		else
			echo FAIL
			sed 's/^/    /' "$LOGDIR/$(basename "$t").out" >&2
			php_fail=1
		fi
	done
	[ "$php_fail" = 0 ] || overall=1
fi

# --- 4. daemonised pre-fork server ---------------------------------------------
step "starting daemonised pre-fork server on 127.0.0.1:$DPORT (4 workers)"
rm -f "$daemon_pid_file"
./yar_test_server -S "127.0.0.1:$DPORT" -n 4 -p "$daemon_pid_file" -l "$LOGDIR/daemon.log"

if ! ./yar_test_client --uri "tcp://127.0.0.1:$DPORT" --probe; then
	echo "FATAL: daemon server did not come up (see $LOGDIR/daemon.log)" >&2
	[ -f "$LOGDIR/daemon.log" ] && cat "$LOGDIR/daemon.log" >&2
	exit 1
fi

step "C concurrent suite (pre-fork daemon)"
./yar_test_client --uri "tcp://127.0.0.1:$DPORT" --concurrent || overall=1

if [ -f "$daemon_pid_file" ]; then
	daemon_pid=$(cat "$daemon_pid_file")
	kill "$daemon_pid" 2>/dev/null
	i=0
	while [ $i -lt 10 ] && kill -0 "$daemon_pid" 2>/dev/null; do
		sleep 1
		i=$((i + 1))
	done
	if kill -0 "$daemon_pid" 2>/dev/null; then
		echo "warning: daemon server (pid $daemon_pid) did not shut down gracefully" >&2
		kill -9 "$daemon_pid" 2>/dev/null
		overall=1
	fi
fi

step "result: $([ "$overall" = 0 ] && echo OK || echo FAILED)"
exit "$overall"
