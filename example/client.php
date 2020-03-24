<?php

$hostname = $argv[1];
$client = new Yar_Client("tcp://" . $hostname);
$client->SetOpt(YAR_OPT_CONNECT_TIMEOUT, 100);

$result = $client->default(array(
    1, 2,
    'a' => 4
));

if (is_array($result)) {
	var_dump($result);
	exit(0);
}
exit(1);
