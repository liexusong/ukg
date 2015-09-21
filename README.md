Unique Key Generate Server
==========================

How to use:<br />
1) Install UKG server
<pre>
$ make
$ ./ukg -D
</pre>

2) Install php extension
<pre>
$ cd ukg_php_extension
$ phpize
$ ./configure
$ make & make install
</pre>

3) Write php script
<pre>
&lt;?php
$key = ukg_getkey('127.0.0.1', 7954);
var_dump($key);
$info = ukg_key2info($key);
var_dump($info);
?&gt;
</pre>
