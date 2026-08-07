<?php
/**
 * PHP client -> C server: JSON packager roundtrip
 *
 * Runs with -d yar.packager=json (see run_all.sh). The server must reply
 * with the same packager, so every payload here travels as JSON text in
 * both directions. Payloads must stay JSON-safe: no binary strings with
 * embedded NUL bytes, numbers within the exact double range.
 */

require __DIR__ . '/helper.php';

assert_same('json', strtolower((string)ini_get('yar.packager')), 'yar.packager ini is json');

$client = yar_test_client();

/* scalar roundtrip (JSON-safe subset, no binary) */
$sent = [42, 4294967296, 3.14, true, false, null, 'héllo 你好'];
$got = $client->call('echo', $sent);
assert_same($sent, $got, 'scalar roundtrip');

/* composite roundtrip */
$sent = [
    'list' => [1, 2, 3],
    'meta' => ['k' => 'v', 'n' => -7],
    'string' => 'hello',
];
$got = $client->call('echo', [$sent]);
assert_same([$sent], $got, 'composite roundtrip');

/* arithmetic handlers */
assert_same(5, $client->call('add', [2, 3]), 'integer add');
assert_same(3.75, $client->call('add', [1.5, 2.25]), 'double add');
assert_same(4.5, $client->call('add', [4, 0.5]), 'mixed add');

/* error envelope travels as JSON too */
try {
    $client->call('no_such_method', []);
    assert_true(false, 'undefined method should throw');
} catch (Throwable $e) {
    assert_true(
        stripos($e->getMessage(), 'undefined method') !== false,
        'unexpected exception message: ' . $e->getMessage()
    );
}

yar_test_done();
