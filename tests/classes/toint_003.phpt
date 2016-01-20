--TEST--
__toInt() must not throw an exception
--FILE--
<?php

class Test
{
	public function __toInt()
	{
		throw new \Exception('This is an exception');
		return 42;
	}
}

$test = new Test();
(int) $test;

?>
--EXPECTF--
Fatal error: Method Test::__toInt() must not throw an exception, caught Exception: This is an exception in %s on line %d

