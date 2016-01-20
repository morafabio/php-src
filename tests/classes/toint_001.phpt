--TEST--
__toInt()
--FILE--
<?php

class Test
{
		public function __toInt()
		{
			return 42;
		}
}

$test = new Test();

var_dump($test->__toInt());
var_dump((int) $test);
var_dump(100 + $test);
var_dump(sprintf("%d" + $test));

?>
--EXPECTF--
int(42)
int(42)
int(142)
string(2) "42"

