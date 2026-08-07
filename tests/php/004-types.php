<?php
/**
 * PHP client -> C server: full type matrix returned by the types handler
 */

require __DIR__ . '/helper.php';

$client = yar_test_client();
$r = $client->call('types', []);

assert_true(is_array($r), 'types returns an array');
/* note: $r['null'] ?? default would also trigger for a present-but-null value */
assert_true(array_key_exists('null', $r) && $r['null'] === null, 'null value');
assert_same(true, $r['true'] ?? null, 'true value');
assert_same(false, $r['false'] ?? null, 'false value');
assert_same(-12345, $r['long'] ?? null, 'long value');
assert_same(4294967296, $r['ulong'] ?? null, 'ulong value');
assert_same(3.14159, $r['double'] ?? null, 'double value');
assert_same('hello yar', $r['string'] ?? null, 'string value');
assert_same("\x00\x01\x02\xff", $r['binary'] ?? null, 'binary value');
assert_same([1, 'two', 3.0], $r['array'] ?? null, 'array value');
assert_same(['nested' => ['deep' => [true, null]]], $r['map'] ?? null, 'nested map value');
assert_same([], $r['empty_array'] ?? null, 'empty array value');
/* pecl msgpack decodes an empty map as stdClass (it cannot tell it was meant
 * to be a PHP array), so accept either shape as long as it is empty */
assert_true(isset($r['empty_map']) && (array)$r['empty_map'] === [], 'empty map value');

yar_test_done();
