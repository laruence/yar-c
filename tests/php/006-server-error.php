<?php
/**
 * PHP client -> C server: server-side application error (status YAR_ERR_EXCEPTION)
 * must surface as a Yar_Server_Exception carrying the original message
 */

require __DIR__ . '/helper.php';

$client = yar_test_client();

try {
    $client->call('error', []);
    assert_true(false, 'error method should throw');
} catch (Yar_Server_Exception $e) {
    assert_same('intentional error from test server', $e->getMessage(), 'exception message');
    assert_same(0x40, $e->getCode(), 'exception code');
} catch (Throwable $e) {
    assert_true(false, 'expected Yar_Server_Exception, got ' . get_class($e) . ': ' . $e->getMessage());
}

yar_test_done();
