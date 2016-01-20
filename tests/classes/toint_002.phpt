--TEST--
__toInt() in __destruct
--FILE--
<?php

class Test
{
	public function __toInt()
	{
		return 42;
	}

	public function __destruct()
	{
		print (int) $this;
	}
}

$test = new Test();
unset($test);
print "\n";

?>
--EXPECTF--
42

