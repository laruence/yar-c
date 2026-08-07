<?php
/**
 * PHP client -> C server: read timeout (YAR_OPT_TIMEOUT is in milliseconds).
 * The server handler sleeps 3s, the client gives up after 1s.
 *
 * NOTE: keep this test last, it blocks the single-process fixture server.
 */

require __DIR__ . '/helper.php';

$client = new Yar_Client(yar_test_uri());
$client->SetOpt(YAR_OPT_CONNECT_TIMEOUT, 5000);
$client->SetOpt(YAR_OPT_TIMEOUT, 1000);

try {
    $client->call('sleep', [3]);
    assert_true(false, 'sleep should time out');
} catch (Throwable $e) {
    assert_true(true, 'timed out as expected: ' . $e->getMessage());
}

yar_test_done();
