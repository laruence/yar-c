<?php
/**
 * PHP client -> C server: add handler
 */

require __DIR__ . '/helper.php';

$client = yar_test_client();

assert_same(5, $client->call('add', [2, 3]), 'integer add');
assert_same(3.75, $client->call('add', [1.5, 2.25]), 'double add');
assert_same(4.5, $client->call('add', [4, 0.5]), 'mixed add');

yar_test_done();
