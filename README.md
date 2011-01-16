**Warning: This is old code (PHP 5.1).**

pebugger
========
pebugger is a debugger for PHP, written in PHP.
It's an interactive command-line based application, that vaguely resembles GDB.

```
$ pebugger
Debugger connected [127.0.0.1]
Xdebug 2.0.0RC4 - http://xdebug.org - Copyright (c) 2002-2007 by Derick Rethans
Status set to 'break'
Break in function {main}:
2:    echo("hello\n");
-> break 16
Breakpoint 1 set: file:///tmp/a.php:16
-> run
hello
Break in function two:
16:        $n++;
-> list
11:    function two($n)
12:    {
13:        $num = 5;
14:        $a = Array('lemon', 'apple', 'orange', 'pear');
15:        $str = 'hello';
16:        $n++;
17:        return three($n);
18:    }
19:
20:    function three($n)
21:    {
-> echo $a[2]
"orange"
-> exit
$
```

Quick Start:
------------

You should have Xdebug working on your test server, and pebugger on your
local machine (or both on the same machine; it doesn't matter).

Now, point your browser to the php script that you want to debug, and
add the following argument to the url:
        
        XDEBUG_SESSION_START=aaa

For example: `http://127.0.0.1/test.php?XDEBUG_SESSION_START=aaa`
(For now, the 'aaa' can be anything you like: eg; 'aaa')


So far all you can do is walk through code, set breakpoints and look at
variable. Type 'help'.



Requirements:
-------------

- Atleast PHP 5
    Not sure of the exact minimum, but I know 5.1.4 works for sure.

- Xdebug: http://www.xdebug.org/
    See below
    
- readline: http://www.php.net/manual/en/ref.readline.php
    Optional; Strongly recommended, especially if you enjoy line editing,
    tab-completion (for commands and variables), and command history.

- CLI support in PHP (it's enabled by default)



Installing:
-----------

Xdebug:

Install Xdebug by doing:

    pecl install xdebug

[Xdebug only needs to be installed on the 'server' machine, not the one you're
running pebugger on, if different]

For more info: http://www.xdebug.org
Don't forget to add something like:

    zend_extension=xdebug.so

to your php.ini and also check the output of phpinfo() if it mentions Xdebug.

Have a look at http://www.xdebug.org/docs/remote on how to get Xdebug to connect
to pebugger.



**Readline Extension**

If you want bash-like line editing, tab-completion and command history, then
the readline extension has to be enabled, which is not by default. This isn't
available for Windows.
To do this, recompile PHP with either '--with-readline' or
'--with-readline=shared' added to the ./configure command. So, a good idea
would be to see what options you previously compiled PHP with by doing:

    php -r 'phpinfo();' | grep 'Configure Command'

and add your --with-readline option to the list.

It might be worth having a look in /usr/lib/php for 'readline.so' before
doing this, just incase you already have it. If so, just enable it in your
php.ini

See also: http://www.php.net/manual/en/ref.readline.php


If you will be debugging remotely, then just xdebug must be installed on the
'server', and readline installed only on the system pebugger is to be ran on.


Installing pebugger
-------------------

Just copy it to wherever you want (say, /usr/local/share/pebugger)
then do something like:

	ln -s /usr/local/share/pebugger/pebugger.php /usr/local/bin/pebugger

Make sure the first line of pebugger.php points to where your php is.
Default is /usr/bin/php
