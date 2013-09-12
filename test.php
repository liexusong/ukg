<?php

$client = fsockopen('127.0.0.1', 7954);

if (!$client) {
    exit('Failed to connect to server');
}

echo fgets($client); /* Get the Unique Key */
