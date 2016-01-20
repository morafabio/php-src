--TEST--
__toInt() with strict_types triggers TypeError
--FILE--
<?php

declare(strict_types=1);

class Test
{
	public function __toInt()
	{
		return 42;
	}
}

$test = new Test();
function beInt(int $x) { }
beInt($test); // Fatal error: Uncaught TypeError 

?>
====DONE====
--EXPECTF--
Fatal error: Uncaught TypeError: Argument 1 passed to beInt() must be of the type integer, object given, called in %s on line %d and defined in %s:%d
Stack trace:
#0 %s(%d): beInt(Object(Test))
#1 {main}
  thrown in %s on line %d
