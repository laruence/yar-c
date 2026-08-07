<?php
/**
 * PHP client -> C server: 1M payload roundtrip
 */

require __DIR__ . '/helper.php';

$client = yar_test_client();

$len = 1048576;
$got = $client->call('big', [$len]);

assert_true(is_string($got), 'big returns a string');
assert_same($len, strlen($got), 'big payload length');

$ok = true;
for ($i = 0; $i < $len; $i += 4096) {
    if ($got[$i] !== chr(ord('a') + ($i % 26))) {
        $ok = false;
        break;
    }
}
assert_true($ok, 'big payload pattern');

yar_test_done();
