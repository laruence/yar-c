<?php
/**
 * PHP client -> C server: persistent link, several calls over one connection
 */

require __DIR__ . '/helper.php';

$client = yar_test_client();
$client->SetOpt(YAR_OPT_PERSISTENT, 1);

for ($i = 0; $i < 5; $i++) {
    assert_same([[$i, 'x']], $client->call('echo', [[$i, 'x']]), "persistent call #$i");
}

yar_test_done();
