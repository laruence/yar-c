<?php
/**
 * PHP client -> C server: composite structure roundtrip through echo
 */

require __DIR__ . '/helper.php';

$client = yar_test_client();

$sent = [
    'list' => [1, 2, 3],
    'meta' => ['k' => 'v', 'n' => -7],
    'string' => 'hello',
];
/* Yar_Client::call($method, $parameters) takes the whole parameter array,
 * so wrap the map to send it as a single positional argument */
$got = $client->call('echo', [$sent]);

assert_same([$sent], $got, 'composite roundtrip');

yar_test_done();
