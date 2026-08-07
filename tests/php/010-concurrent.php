<?php
/**
 * PHP client -> C server: concurrent callers
 *
 * Forks several worker processes that all call the server in parallel and
 * verify every echoed payload. Requires ext-pcntl; skipped when it is not
 * available (pcntl is a CLI-only extension and not part of every build).
 */

require __DIR__ . '/helper.php';

$workers = 8;
$calls = 20;

if (!function_exists('pcntl_fork') || !function_exists('pcntl_waitpid')) {
    fwrite(STDOUT, "SKIP: ext-pcntl is not available\n");
    exit(0);
}

$pids = [];
for ($w = 0; $w < $workers; $w++) {
    $pid = pcntl_fork();
    if ($pid === -1) {
        fwrite(STDERR, "FAIL: fork failed\n");
        exit(1);
    }
    if ($pid === 0) {
        /* child: build a fresh client, never reuse the parent's state */
        $client = yar_test_client();
        for ($i = 0; $i < $calls; $i++) {
            $payload = "worker $w call $i";
            $got = $client->echo($payload);
            if ($got !== [$payload]) {
                fwrite(STDERR, "FAIL: worker $w call $i roundtrip mismatch, got "
                    . var_export($got, true) . "\n");
                exit(1);
            }
        }
        exit(0);
    }
    $pids[] = $pid;
}

$failed = 0;
foreach ($pids as $pid) {
    pcntl_waitpid($pid, $status);
    if (!pcntl_wifexited($status) || pcntl_wexitstatus($status) !== 0) {
        $failed++;
    }
}

assert_true($failed === 0, "all $workers concurrent workers succeeded ($failed failed)");

yar_test_done();
