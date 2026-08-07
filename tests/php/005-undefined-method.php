<?php
/**
 * PHP client -> C server: calling an undefined method must raise an error
 */

require __DIR__ . '/helper.php';

$client = yar_test_client();

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
