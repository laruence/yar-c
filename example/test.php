<?php

$client = new Yar_Client("tcp://192.168.1.124:9999");
$client->SetOpt(YAR_OPT_CONNECT_TIMEOUT, 100);

$result = $client->default(array(
    1, 2,
    'a' => 4
));

var_dump($result);