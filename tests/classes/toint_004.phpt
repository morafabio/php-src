--TEST--
__toInt() must return a string
--FILE--
<?php

class Test
{
	public function __toInt()
	{
		return "Oh no! I am a string!";
	}
}

$test = new Test();
(int) $test;

?>
--EXPECTF--
Catchable fatal error: Method Test::__toInt() must return an int value in %s on line %d
