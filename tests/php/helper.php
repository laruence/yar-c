<?php
/**
 * Yar C test suite - PHP client test helper
 *
 * Every test file includes this helper, talks to the fixture server whose
 * address is given via the YAR_TEST_URI environment variable (eg.
 * "tcp://127.0.0.1:19871") and exits 0 on success / 1 on failure.
 */

error_reporting(E_ALL);

$GLOBALS['yar_test_failed'] = 0;

function yar_test_uri(): string
{
    $uri = getenv('YAR_TEST_URI');
    if (!$uri) {
        fwrite(STDERR, "YAR_TEST_URI is not set\n");
        exit(2);
    }
    return $uri;
}

function yar_test_client(): Yar_Client
{
    $client = new Yar_Client(yar_test_uri());
    $client->SetOpt(YAR_OPT_CONNECT_TIMEOUT, 5000); /* milliseconds */
    $client->SetOpt(YAR_OPT_TIMEOUT, 5000);         /* milliseconds */
    return $client;
}

function assert_true($cond, string $what): void
{
    if (!$cond) {
        $GLOBALS['yar_test_failed'] = 1;
        fwrite(STDERR, "FAIL: $what\n");
    }
}

function assert_same($expected, $actual, string $what): void
{
    if ($expected !== $actual) {
        $GLOBALS['yar_test_failed'] = 1;
        fwrite(STDERR, "FAIL: $what\n"
            . '  expected: ' . var_export($expected, true) . "\n"
            . '  actual:   ' . var_export($actual, true) . "\n");
    }
}

function yar_test_done(): void
{
    exit($GLOBALS['yar_test_failed'] ? 1 : 0);
}
