<?php
/**
 * Contains code for the commands that are entered by the user
 * 
 * @package pebugger
 */   

/**
 * A command
 */ 
class Command
{
    /**
     * The name of the command
     */         
    public $name;
    /**
     * min/max number of arguments required.
     * ignored for aliases     
     */         
    public $min = -1, $max = -1;
    /**
     * is a debugger required to be connected?
     */         
    public $dbg = FALSE;
    /**
     * The command-line that this alias expands to
     * Unset for a built-in command     
     */         
    public $alias;
    /**
     * The command's usage
     */
    public $usage;
    /**
     * A description of the command
     */
    public $hint;
    /**
     * The function to call; 'cmd_$func' will be called
     */
    public $func;
    
    /**
     */
    public function __construct($min=0, $max=0, $dbg=FALSE, $hint='', $usage='', $func = '')
    {
        if (func_num_args() > 0) {
            $this->min = $min;
            $this->max = $max;
            $this->dbg = $dbg;
            $this->usage = $usage;
            $this->hint = $hint;
            if (empty($func)) {
                $this->func = &$this->name;
            } else {
                $this->func = $func;
            }
            unset($this->alias);
        }
    }
    public function setAlias($alias)
    {
        unset($this->min);
        unset($this->max);
        $this->dbg = FALSE;
        $this->alias = $alias;
    }
    
}

/**
 * command creation and calling
 */ 
class Cmd {

    public $debugger = FALSE;
    public $console;

    /**
     * Array of the commands.
     */
    private $commands = Array();
    /**
     * Sorted list of command names
     */         
    public $commandList = Array();

    /**
     * the last command (or direct alias)
     */         
    private $lastCmd = '';
    private $cmd = '';

    /**
     * if TRUE, a call to a cmd_* function must only return a help string
     */     
    private $getHelp = FALSE;

    public function __construct($console)
    {
        $this->console = $console;
        $this->debugger = FALSE;
        
        readline_completion_function(Array($this, 'readlineCompletion')); 

        /* built-in commands */
        $this->commands = Array(
        
            'status' => new Command(0, 0, TRUE, 'Returns the current debugger state'),
            'backtrace' => new Command(0, 1, TRUE, 'Display the current stack'),
            'list' => new Command(0, 2, TRUE, 'Lists the specified line(s) of source code', '[line] [cnt]'),
            'step' => new Command(0, 0, TRUE, 'Execute the next line, going inside functions', '', 'step'),
            'stepover' => new Command(0, 0, TRUE, 'Execute the next line, going over functions', '', 'step'),
            'stepout' => new Command(0, 0, TRUE, 'Execute until the current function returns', '', 'step'),
            'break' => new Command(1, 1, TRUE, 'Sets a break point at the line or function',
                '[file:]<line|function>'),
            'detach' => new Command(0, 0, TRUE, 'Detach from the debugger, and let the script continue running'),
            
            'start' => new Command(1, 1, FALSE, 'Starts and debugs a script', '<script file or url>' ),
            
            'vars' => new Command(0, 1, TRUE, 'Displays all variables available to the current frame', '[-v]', 'vars'),
            'locals' => new Command(0, 1, TRUE, 'Displays all local variables', '[-v]', 'vars'),
            'globals' => new Command(0, 1, TRUE, 'Displays all global variables', '[-v]', 'vars'),
            'class' => new Command(0, 1, TRUE, 'Displays all class variables', '[-v]', 'vars'),
            
            'echo' => new Command(0, 1, TRUE, 'Display the value of an expression, in the current context of the script', '[expr]'),
            'print' => new Command(0, 1, TRUE, "Same as 'echo', but the return is sent to the script's output", '[expr]', 'echo'),
            'eval' => new Command(0, 1, TRUE, "Evaluates an expression in the current context of the script", '[expr]', 'echo'),
            
            'set' => new Command(0, 2, FALSE, 'Assign and display settings', '[setting [value]]'),
            'unset' => new Command(1, 1, FALSE, 'Unsets a setting. If the setting is built-in, then it is reverted to the default', 'setting'),
            'alias' => new Command(0, 2, FALSE, 'Create/update/remove an alias command', '[name [command-line]]'),
            'showcommands' => new Command(0, 0, FALSE, 'Show all the available commands'),
            'help' => new Command(0, 1, FALSE, 'Displays information on a command', '[command]'),

            'about' => new Command(0, 1, FALSE, 'Displays information about pebugger', ''),

            'exit' => new Command(0, 1, FALSE, 'Terminate pebugger', ''),


        );
        
        foreach ($this->commands as $k => &$cmd) {
            /* set the names to the key's text */
            $cmd->name = $k;
            /* add it to the list (it will be sorted when aliases are added) */
            $this->commandList[] = $k;
        }

        /* aliases */
        $a = Array(
            'bt' => 'backtrace',
            'l' => 'list',
            'si' => 'step',
            '?' => 'echo',
            'quit' => 'exit',
            'bye' => 'exit'
        );
        $this->addAlias($a);
        
    }


    /**
     * Defines an alias
     * @param mixed $aliasName array of 'name=>alias', or the alias name
     * @param string $alias The command line the alias calls
     * @return Command The Command object of the alias
     */     
    public function addAlias($aliasName, $alias='')
    {
        static $x = 0;
        $x++;
        if (is_array($aliasName)) {
            foreach ($aliasName as $n => $a) {
                $this->addAlias($n, $a);
            }
            $c = NULL;
        } else {
            $c = new Command();
            $c->setAlias(trim($alias));
            $c->name = trim($aliasName);
            $this->commands[$c->name] = $c;
            $this->commandList[] = $c->name;
            
            if (strpos($c->name, ' ') === FALSE) {
                /* if this alias directly calls a command, then use
                 * the target command's attributes
                 */
                if (isset($this->commands[$c->alias])) {
                    $t = $this->commands[$c->alias];
                    $c->min = $t->min;
                    $c->max = $t->max;
                    $c->dbg = $t->dbg;
                    $c->usage = $t->usage;
                    $c->hint = $t->hint;
                }
            }
        }
        
        if (!--$x) {
            sort($this->commandList);
        }
        return $c;
    }
    /*
     * get the commands that begin with $cmd
     * @param string $cmd the beginning part of the command
     * @return array the commands that match
     */     
    public function completeCommand($cmd)
    {
        /* make sure it contains 'normal' characters (and regex safe) */
        if (preg_match('/[^a-zA-Z0-9-]/', $cmd))
            return Array();
        
        return preg_grep("/^$cmd/", $this->commandList);
    }
    
    /**
     * fills an array for tab-completion
     * @param string $word the current word
     * @param int $s the start position of the word
     * @param int $e position of the cursor
     * @return array A list of possible completes     
     */
    public function readlineCompletion($word, $s, $e)
    {
        $line = readline_info('line_buffer');
        
        /* trim the line, and move the pointers back */
        $len = strlen($line);
        $line = trim($line);
        $diff = $len - strlen($line);
        $s -= $diff;
        $e -= $diff;
        
        /* see if the tab is hit on the first word (the command) */
        if ($s == 0) {
            /* get the commands that begin with $word */
            $r = $this->completeCommand($word);
            
            if (count($r) >0)
                return $r;
        }

        if (is_object($this->debugger)) {
            /* variables */
            if (substr($line, $s-1, 1) == '$') {
                $vars = Array();
                for ($i = 0; $i < 3; $i++) {
                    $r = $this->debugger->cmd('context_get', "-c $i", TRUE, '[tab]', FALSE);
                    if (!isset($r['PROPERTY'])) {
                        return Array();
                    }
                    $props[$i] = $r['PROPERTY'];
                    if (!isset($props[$i][0])) {
                        $tmp = $props;
                        $props[$i] = Array(0 => $tmp);
                    }
                    foreach ($props[$i] as $p) {
                        $vars[] = $p['_attr']['NAME'];
                    }
                }
                return $vars;
            }
        }

        //$this->console->writeln("'$line' '$word' $s $e");
        

        
        return '';

        
    }
    
    /**
     * executes a command
     * @param string $cmdline The command line
     **/   
    public function callCommand($cmdline)
    {
        $cmdline = trim($cmdline);
        
        /* ignore blank lines, and comments */
        if (strlen($cmdline) == 0)
            return;
        if (substr($cmdline, 0, 1) == '#')
            return;
        
        $a = explode(' ', $cmdline, 2);
        $cmd = $a[0];
        if (isset($a[1]))
            $argstr = trim($a[1]);
        else
            $argstr = '';


        if (isset($this->commands[$cmd])) {
            $c = $this->commands[$cmd];
            /* put the args into an array */
            $a = explode(' ', $argstr);

            if ($c->dbg && !$this->debugger) {
                $this->console->writeln('Not connected');
                return;
            }
            
            /* get the limit of array elements */
            if (isset($c->min)) {
                if (empty($this->lastCmd)) {
                    $this->lastCmd = $cmd;
                }

                if (($m = $c->max) < $c->min) {
                    if ($m < 0) {
                        $m = 100;
                    } else {
                        $m = $c->min;
                    }
                }
            } else {
                $m = 1;
            }
            
            $argstr = trim($argstr);
            if (strlen($argstr)) {
                $args = explode(' ', $argstr, $m);
                $argc = count($args);
            } else {
                $argc = 0;
                $args = Array();
            }

            if (isset($c->min) && ($argc < $c->min)) {
                $this->console->writeln("$cmd: not enough args");
                $this->putUsage("Usage:", $c);
                return;
            }

            if (isset($c->alias)) {
                /* it's an alias */
                $this->callCommand($c->alias . " $argstr");
                return;
            } else {
                $this->cmd = $cmd;
                if ($argstr == '') {
                    call_user_func(Array($this, 'cmd_' . $c->func));
                } else {
                    call_user_func_array(Array($this, 'cmd_' . $c->func), $args);
                }
                $this->cmd = '';
                $this->lastCmd = '';
                return;
            }
        }
        
        /* the command still can't be found, so try to complete it */
        $a = $this->completeCommand($cmd);
        switch (count($a)) {
        case 0:
            /* no command resembles what was typed */
            if (!$this->debugger) {
                $this->console->writeln('Not connected');
            } else {
                
                $this->debugger->cmd($cmdline, '', FALSE, $this->lastCmd = $cmd);
            }
            return;

        case 1:
            /* got one command that matches */
            $this->callCommand(current($a) . " $argstr");
            return;
        }
        $this->console->writeln("Ambiguous command '$cmd': " . implode(', ', $a));

        
    }
    
    /**
     * display the usage for a command
     * @param string $message Something to say     
     * @param mixed $cmd Either the command name, or a Command class
     */
    public function putUsage($message = 'Usage:', $cmd = '')
    {
        if (is_string($cmd)) {
            if (empty($cmd)) {
                $cmd = $this->cmd;
            }
            if (!isset($this->commands[$cmd])) {
                return;
            }
            $cmd = $this->commands[$cmd];
        }
        if (isset($cmd->usage) && !empty($cmd->usage)) {
            if (!empty($message)) {
                $s = "$message ";
            } else {
                $s = '';
            }
            $this->console->writeln($s . $cmd->name . " " . $cmd->usage);
        }
            
    }

    private function cmd_about($w = NULL)
    {
        global $COPYRIGHT, $VERSION;
        
        if (substr($w, 0, 1) == 'd') {
            if (!is_object($this->debugger)) {
                $this->console->writeln("No debugger connected");
                return;
            }
            $r = $this->debugger->initData;

            foreach ($r as $k => $v) {
                if (substr($k, 0, 1) != '_') {
                    @$s = $k . ': ' . $v['_data'];
                    if ($k == 'ENGINE') {
                        if (isset($v['_attr']['VERSION'])) {
                            $s .= ' ' . $v['_attr']['VERSION'];
                        }
                    }
                    $this->console->writeln($s);
                }
            }
            foreach ($r['_attr'] as $k => $v) {
                if (substr($k, 0, 1) != '_') {
                    @$s = $k . ': ' . $v;
                    $this->console->writeln($s);
                }
            }
            
            
        } else {
            $this->console->writeln(
                "pebugger version $VERSION\n".
                "$COPYRIGHT\n".
                "Type 'about debugger' to read about the connected debugger engine\n"
            );
        }
    }
    
    private function cmd_help($cmd = '')
    {
        if (empty($cmd)) {
            /* no command specified - just list the commands and hints */
            $s = '';
            foreach ($this->commands as $c) {
                $s .= $c->name . ': ' . $c->hint . "\n";
            }
            $this->console->writeln($s);
            return;
        }

        if (substr($cmd, 0, 4) == 'set ') {
            /* show the help for the settings */
            global $SETTINGS;
            $set = trim(substr($cmd, 4));
            if (isset($SETTINGS[$set])) {
                $this->console->writeln($SETTINGS[$set]->getHelp(TRUE));
            } else {
                $this->console->writeln("Unknown setting '$set'");
            }
            return;
        }


        if (isset($this->commands[$cmd])) {
            $c = $this->commands[$cmd];
        } else {
        
            /* try and complete it */
            $a = $this->completeCommand($cmd);
            switch (count($a)) {
            case 0:
                $this->console->writeln("No such command '$cmd'.");
                return;
    
            case 1:
                /* got one command that matches */
                $c = $this->commands[current($a)];
                break;
            default:
                $this->console->writeln("Ambiguous command '$cmd': " . implode(', ', $a));
                return;

            }
        }
        
        if (isset($c)) {

            if (isset($c->alias)) {
                /* it's an alias - show the help for the target command */
                $a = strtok($c->alias, ' ');
                if (isset($this->commands[$a])) {
                    if (empty($this->commands[$a]->alias)) {
                        $c = $this->commands[$a];
                    }
                }
            }
        
            if (empty($c->usage) && empty($c->hint)) {
                $this->console->writeln("No help available for '$cmd'");
                return;
            }
            $s = 'Usage: ' . $c->name . ' ' . $c->usage;
            if (!empty($c->hint)) {
                $s .= "\n" . $c->hint;
            }
            
            $this->console->writeln("$s");
            
            /* find aliases to this command */
            $x = Array();
            foreach ($this->commands as $n => $a) {
                if (isset($a->alias)) {
                    if ($a->alias == $c->name) {
                        $x[] = $n;
                    }
                }
            }
            
            if (($n = count($x)) > 0) {
                $p = ($n > 1) ? 'es' : '';
                $this->console->writeln("Alias$p: " . implode(', ', $x));
            }
            
        }

       
    }
    
    private function cmd_alias($name = '', $cmd = '')
    {
        if ($name == '') {
            /* list all aliases */
            foreach ($this->commands as $n => $c) {
                if (isset($c->alias)) {
                    $this->console->writeln("$n: " . $c->alias);
                }
            }
        } elseif ($cmd == '') {
            /* remove the alias */
            if (isset($this->commands[$name]) && isset($this->commands[$name]->alias)) {
                unset($this->commands[$name]);
                $this->console->writeln("Alias '$name' removed");
            } else {
                $this->console->writeln("No such alias '$name'");
            }
        } else {
            /* create/update the alias */
            if (isset($this->commands[$name])) {
                $c = $this->commands[$name];
                if (!isset($c->alias)) {
                    $this->console->writeln("I will not replace the command '$name'");
                    return;
                }
                $c->setAlias($cmd);
            } else {
                $c = $this->addAlias($name, $cmd);
            }
            $this->console->writeln("Created alias '" . $c->name . "': " . $c->alias);
        }

    }

    private function cmd_start($uri = '')
    {
        if ($uri == '')
            return;
            
        $session = 'aaa';
        
        $u = parse_url($uri);
        if (isset($u['scheme'])) {
            if ($u['scheme'] == 'http') {
                /* point a browser to the location */
                
                $s = "?&XDEBUG_SESSION_START=$session";
                $url = "$uri$s";
                $fmt = setting('browser');
                if (strpos($fmt, '%s') === FALSE) {
                    $cmd = "$fmt '$url'";
                } else {
                    $cmd = sprintf($fmt, "'$url'");
                }
                system("nohup $cmd >/dev/null &");
                
            } elseif ($u['scheme'] == 'file') {
                if (isset($u['path'])) {
                    $f = $u['path'];
                } else {
                    $this->console->writeln("Error in URI '$uri'");
                }
            } else {
                $this->console->writeln("Unsupported URI scheme '" . $u['scheme'] . "'");
                return;
            }
            
        } else {
            $f = $uri;
        }

        /* start the php script */
        putenv("XDEBUG_CONFIG=$session");
        system("nohup php '$f' >/dev/null  &");
        
    }

    private function cmd_unset($name)
    {
        global $SETTINGS;
        if (isset($SETTINGS[$name])) {
            if ($SETTINGS[$name]->builtin) {
                $SETTINGS[$name]->value = $SETTINGS[$name]->default;
                $s = "Set '$name' back to default setting.";
            } else {
                unset($SETTINGS[$name]);
                $s = "Removed setting '$name'.";
            }
        } else {
            $s = "No such setting '$name'";
        }
        $this->console->writeln($s);
    }

    private function cmd_set($name = '', $value = '')
    {
        global $SETTINGS;
        if ($name == '') {
            /* list all settings */
            foreach ($SETTINGS as $s) {
                $this->console->writeln($s->name . ': ' . $s->value);
            }
        } else if ($name == 'help') {
            if ($value != '') {
                /* show help for a setting */
                $this->callCommand("help set $value");
            } else {
                /* show breif help for all settings */
                foreach ($SETTINGS as $s) {
                    $this->console->writeln($s->name . ': ' . $s->getHelp());
                }
            }
                
        } else {
            if (($p = strpos($name, '=')) !== FALSE) {
                /* handle 'set name=value' */
                $value = trim(substr($name, $p+1) . ' ' . $value);
                $name = substr($name, 0, $p);
                if ($value == '') {
                    $this->cmd_unset($name);
                    return;
                }
            }
            if ($value == '') {
                /* display the value */
                if (isset($SETTINGS[$name])) {
                    $this->console->writeln(
                        $SETTINGS[$name]->name . ' = ' . $SETTINGS[$name]->value
                    );
                } else {
                    $this->console->writeln("No such setting '$name'");
                }
            } else {
                /* set the value */
                setSetting($name, $value);
                $this->console->writeln("Set $name to '$value'");
            }
        }
    }
    
    private function cmd_step()
    {
    
        switch($this->cmd) {
        case 'stepover':
            $c = 'step_over';
            break;
        case 'stepout':
            $c = 'step_out';
            break;
        case 'step':
        default:
            $c = 'step_into';
            break;
        }
        $this->debugger->cmd($c, '', FALSE, $this->lastCmd);
    }
    
    private function cmd_break($loc = '')
    {
        if (empty($loc)) {
            /* show the current breaks */
        }
        
        /* [file:]<line|function> */
        
        if (($p = strpos($loc, ':')) !== FALSE) {
            $file = substr($loc, 0, $p);
            $loc = substr($loc, $p+1);
        } else {
            $file = '';
        }
        
        $type = ctype_digit($loc) ? 'line' : 'call';
        
        $args = "-t $type ";
        if (empty($file)) {
            $file = $this->debugger->filename;
        }
        $args .= "-f $file ";
            
        if ($type == 'line')
            $args .= '-n ';
        else
            $args .= '-m ';
        $args .= $loc;
        
        $args .= ' -c 1';
        $r = $this->debugger->cmd('breakpoint_set', $args, TRUE);
        
        
        
        
    }
    
    private function cmd_status()
    {
        $this->debugger->cmd('status', '', FALSE, $this->lastCmd);
    }
    private function cmd_backtrace($num = '')
    {
        if (!is_numeric($num)) {
            $this->putUsage();
            return FALSE;
        }
        $this->debugger->cmd('stack_get', $num, FALSE, $this->lastCmd);
    }
    private function cmd_list($centre = -1, $num = 5)
    {
        if (!is_numeric($centre) || !is_numeric($num)) {
            $this->putUsage();
            return FALSE;
        }
        if ($centre == -1) {
            if (($this->debugger->lineOut > 0) && ($this->debugger->lineOut != $this->debugger->line)) {
                $centre = $this->debugger->lineOut + $num + 1;
            } else {
                $centre = $this->debugger->line;
            }
        }
        $start = $centre - $num;
        if ($start < 0) {
            $start = 1;
            
        }
        $last = $centre + $num + 1;
        $f = $this->debugger->filename;
        if ($this->debugger->getSource($f) === FALSE)
            return FALSE;
        
        $this->debugger->printSource($f, $start, $last - $start);
        
        
    }
    
    private function cmd_vars($v = '')
    {
        if (($v != '-v') && !empty($v)) {
            $this->putUsage();
            return FALSE;
        }

        switch ($this->cmd) {
        case 'locals':
            $c = '0';
            $t = 'local';
            break;
        case 'globals':
            $c = '1';
            $t = 'global';
            break;
        case 'class':
            $c = '2';
            $t = 'class';
            break;
        case 'vars':
            return TRUE;
        default:
            return FALSE;
        }
        
        if ($v == '-v') {
            $t .= ':';
        }
        
        $this->debugger->cmd('context_get', "-c $c", FALSE, $this->lastCmd, TRUE, $t);  
        
    }
    
    private function cmd_showcommands()
    {
        $this->console->writeln(implode(', ', $this->commandList)); 
    }
    private function cmd_detach()
    {
        $this->debugger->cmd('detach', '', FALSE, $this->lastCmd);
    }

    private function cmd_echo($expr='')
    {
        if (trim($expr) == '') {
            $this->console->writeln('');
            return;
        }
        if ($this->cmd == 'print') {
            $expr = "print $expr";
        }

        $b = '-- ' .  base64_encode($expr);
        $r = $this->debugger->cmd('eval', $b, TRUE, $this->lastCmd);
        if ($r === FALSE)
            return;
        if (!isset($r['PROPERTY']))
            return;
        $p = $r['PROPERTY'];
    
        if ($this->cmd == 'echo') {
            $this->console->writeln($this->debugger->getVariableInfo($p, TRUE));
        }
    }

    private function cmd_exit()
    {
        global $DIE;
        $DIE = TRUE;
    }
}

?>
