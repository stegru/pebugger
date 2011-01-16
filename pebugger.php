#!/usr/bin/php
<?php
$VERSION = '0.3';
$COPYRIGHT = <<<END
pebugger

Copyright (c) 2007, Steven Grundell <pebugger@grundell.co.uk>
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

  Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

  Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

  Neither the name of the pebugger software nor the names of its contributors
  may be used to endorse or promote products derived from this software without
  specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

END;


if (php_sapi_name() != 'cli') {
    echo("pebugger $VERSION\n\n");
    echo("Sorry, this script can only be run from the command line\n");
    exit();
}



/**
 * pebugger
 *
 * @package pebugger
 */  


require_once('config.inc');
require_once('cmd.inc');
require_once('dbgp.inc');
require_once('misc.inc');

if (!($GOT_READLINE = function_exists('readline'))) {
    require_once('readline.inc');
}



if (defined('STDIN'))
    $stdin = STDIN;
else
    $stdin = fopen("php://stdin", "r");





/**
 * Contains the code to do with user input/output
 */ 
class Console
{
    /**
     * the current prompt
     */         
    public $prompt = '-> ';
    
    public $debugger;
    public $cmd;
    private $promptShown = 0;
    private $last = '';
    /* don't display anything in writeln */
    public $hide = FALSE;
    
    public $gettingInput = FALSE; // true if inside gotInput() 
    
    function __construct()
    {
        $this->cmd = new Cmd($this);
        $this->cmd->debugger = &$this->debugger;

        readline_callback_handler_install($this->prompt, Array($this, "gotInput"));

        $this->promptShown = TRUE;
        
        global $VERSION;
        $this->writeln("pebugger $VERSION, Copyright (C) 2007 Steven Grundell.\n" .
"pebugger comes with ABSOLUTELY NO WARRANTY. This is free software, and you are\n" .
"welcome to change and/or distribute copies of it under certain conditions.\n" .
"Type 'about' for more details.\n"
        );
        
        
    }

    
    
    function showPrompt()
    {

        if (!$this->promptShown) {
            readline_on_new_line();
            readline_redisplay();

            $this->promptShown = TRUE;
        }
    }
    
    /**
     * call-back for readline
     */
    function gotInput($in)
    {
        $in = trim($in);
        
        if ($in == '') {
            return;
        }

        $this->gettingInput = TRUE;

        $this->cmd->callCommand($in);
        if ($GLOBALS['DIE']) {
            readline_callback_handler_remove();
            return;
        }
        readline_add_history($in);
        readline_callback_handler_install($this->prompt, array($this, "gotInput"));
        $this->gettingInput = FALSE;
    }

    /**
     * prints a line to stdout
     */         
    function writeln($s)
    {

        if ($this->hide)
            return;
        if ($this->promptShown && !$this->gettingInput) {
            /* something will be displayed, so remove the prompt */

            // see how long the text is 
            $l = readline_info('end') + strlen($this->prompt);
            // remove it
            $this->write("\r" . str_repeat(' ', $l) . "\r");
            $this->promptShown = FALSE;
        }

        $this->write("$s\n");
    }
    /**
     * prints something
     */         
    private function write($s)
    {
        if ($this->hide)
            return;
        echo($s);
    }
}

function shutdown()
{
    global $listen, $sck;
    saveConfig();
    @fclose($listen);
    @fclose($sck);
}

register_shutdown_function('shutdown');

$console = new Console();

$DIE = FALSE;

if (isset($argv[1])) {
    $start_file = $argv[1];
}

readConfig();

$listen = $sck = NULL;
while (1) {

    if (isset($console->debugger)) {
        while ($console->debugger->pendingData) {
            /* there's still something waiting to be processed from the debugger */
            $console->debugger->getData();
        }
    }

    if (($listen == NULL) && ($sck == NULL)) {
        /* listen for incomming connections */
        $ip = setting('bind');
        $port = setting('port');
        $listen = stream_socket_server("tcp://$ip:$port");
    }

    if (isset($start_file)) {
        $console->gotInput("start $start_file");
        unset($start_file);
    }

    /* make an array of streams */
    $fds = array('stdin' => $stdin, 'listen' => $listen, 'sck' => $sck);    
    unset($read);
    foreach ($fds as $n =>$v) {
        if (isset($v) && ($v != NULL)) {
            $read[$n] = $v;
        }
    }
    
    if (!isset($read)) {
        /* there's no streams to read! */
        die("nothing to do\n");
    }

    $console->showPrompt();
    

    /* wait for something to happen */
    $w = $e = NULL;
    if (($r = stream_select($read, $w, $e, 60)) === false) {
        die("select");
    }
    
    if ($r == 0) {
        /* TODO: check if there's still a connection */
        continue;
    }
    
    foreach ($read as $fd) {
        $fd_name = array_search($fd, $fds);

        if ($fd_name == 'stdin') {
            /* let readline handle use input */
            readline_callback_read_char();

        } elseif ($fd_name == 'sck') {

            if (feof($fd)) {
                $ret = FALSE;
            } else {
                $ret = $console->debugger->getData();
            }
            if ($ret === FALSE) {
                /* EOF - close this fd */
                @fclose($fd);
                $sck = NULL;
                $console->debugger = NULL;
                $console->writeln("Debugger Disconnected");
            }

        } elseif ($fd_name == 'listen') {
            /* accept the incomming connection */
            $sck = stream_socket_accept($listen);
            /* don't let any more in */
            fclose($listen);
            $listen = NULL;
            $p = setting('accept');
            if (!empty($p)) {
                $ip = strtok(stream_socket_get_name($sck, TRUE), ':');
                if (!wildMatch($p, $ip)) {
                    /* try to match against the hostname */
                    $h = gethostbyaddr($ip);
                    if (!wildMatch($p, $h)) {
                        $console->writeln("Rejected connection from $h [$ip]");
                        fclose($sck);
                        $sck = NULL;
                        break;
                    }
                }
            }

            $console->debugger = new Debugger(&$sck, $console);
        }
        
        if ($DIE) {
            exit();
        }
        
    }
}




?>
