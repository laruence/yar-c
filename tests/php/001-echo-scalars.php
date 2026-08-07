<?php
/**
 * PHP client -> C server: scalar roundtrip through echo
 */

require __DIR__ . '/helper.php';

$client = yar_test_client();

$sent = [42, 3.14, true, false, null, 'héllo 你好'];
$got = $client->call('echo', $sent);

assert_same($sent, $got, 'scalar roundtrip');

yar_test_done();
