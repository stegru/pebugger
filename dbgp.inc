<?php


/**
 * Contains anything to do with the debugger
 * 
 * @package pebugger  
 */ 


/**
 * Code for the source code
 */ 
class Source
{
    public $file;
    public $lines = Array();
    public $lineCnt = 0;
    
    public function __construct($file, $r=FALSE)
    {
        $this->file = $file;
        if (!$r)
            return;
        $this->setSource($r);
    }
    /**
     * Returns some source code
     * @param int $start the first line to return
     * @param int $end the last line to return. Ommit to return just 1 line
     * @return array The lines. The keys are the actual line numbers
     */                        
    public function getLines($start, $end = -1)
    {
        if ($end < 0)
            $end = $start;
        
        $a = Array();
        for ($i = $start; $i <= $end; $i++) {
            $a[$i] = $this->lines[$i];
        }
        return $a;
    }
    
    /**
     * Sets the source code
     * @param mixed $r The response xml element
     */
    public function setSource($r)
    {
        
        /* put a line in the top, so the indexes start at 1 */
        $text = "\n" . $r['_data'];

        $this->lines = preg_split("/[\n]/", $text);
        
        /* get rid of the first dummy line */
        unset($this->lines[0]);
        
        $this->lineCnt = count($this->lines);

    }
}

class Breakpoint
{
    public $type = '';
    public $enabled = TRUE;
    public $file = '';
    public $line = -1;
    public $func = '';
    public $temp = FALSE;
    public $num = 0;
    public $id = 0;

    public function __construct($id)
    {
        $this->id = $id;
    }
    public function summary($new = FALSE)
    {
        switch ($this->type) {
        case 'line':
            $pos = $this->line;
            break;
        case 'call':
            $pos = $this->func;
            break;
        default:
            $pos = '???';
        }
        $d = $this->enabled ? '' : '(disabled)';
        return "Breakpoint $this->id $d" . ($new?'set':'at') . ": $this->file:$pos";
    }
    
}

/**
 * Handles the debugger
 */
class Debugger
{
    private $sck;
    private $buffer = '';

    public $initData;

    public $console;

    public $pendingData = FALSE;

    /* debugger states */
    public $status = 'stopped';
    /**
     * Current line of execution
     */         
    public $line = 0;
    /**
     * Current line displayed
     */         
    public $lineOut = 0;
    /**
     * Current file being run
     */         
    public $filename = '';
    /**
     * Current function
     */
    public $func = '';         
    /**
     * Currently loaded source files
     */         
    public $source = Array(/* filename => Source */);
    
    public $breakpoint = Array();
    public $nextBreakpointID = 1;

    function __construct($sck, $console)
    {
        $this->sck = &$sck;
        $this->console = $console;
        $ip = strtok(stream_socket_get_name($this->sck, TRUE), ':');
        $this->console->writeln("Debugger connected [$ip]");
    }
    
    function __destruct()
    {
        unset($this->console->debugger);
    }


    /**
     * Send something to the debugger, as-is.
     * @param string $s what to send
     */          
    function send($s)
    {
        if (($s = trim($s)) == '')
            return;
        //echo(">>> $s\n");
        fwrite($this->sck, $s . "\x0");    
    }


    /**
     * Send a command to the debugger
     *               
     * @param string $cmd The command's name
     * @param string $args The command's arguments
     * @param bool $wait Wait for the reply     
     * @param string $userCmd the command the user typed for this to be sent
     * @param string $show_reply display the reply to the user (in parseData)
     * @param string $tag something to be returned int he transaction id     
     * @return array The response, FALSE on error, NULL for no response
     *      
     */
    function cmd($cmd, $args='', $wait=FALSE, $userCmd='', $show_reply = TRUE, $tag = '')
    {
        $cmd = trim($cmd);
        $args = trim($args);

        if (strlen($cmd) == 0)
            return NULL;
        
        /* make the transaction id */
        // "pebugger-#.cmd.tag.hide
        $t = 'pebugger-' . ++$this->transID;
        $t .= ".$userCmd.$tag";
        if (!$show_reply)
            $t .= '.1';
               
        $this->send("$cmd -i $t $args");
      
        if (!$wait) {
            return NULL;
        }
        
        $r = $this->getResponse($t);
        if (isset($r['ERROR']))
            return FALSE;
        return $r;
    }
    
    /**
     * waits for the server to return a response
     * @param string $tID the transaction ID to wait for
     * @return array the xml packet, FALSE on error     
     */
    function getResponse($tID)
    {
        do {
            $r = $this->getData(TRUE);
            if ($r === FALSE)
                return FALSE;
            
        } while (($r == NULL) || ($r['_attr']['TRANSACTION_ID'] != $tID));
        
        return $r;
        
    }
    /**
     * gets the next packet from the debugger
     * @param bool $wait If true, then wait until a full packet is received
     * @return array first xml packet; NULL if nothing, FALSE on an error (like EOF)
     */         
    function getData($wait = FALSE)
    {
        
        while (!feof($this->sck)) {
            
            if ($this->pendingData) {
                /* don't read in more data until the current packets are dealt with */
                $buf = '';
            } else {
                $buf = fread($this->sck, 8192);
                if ($buf === FALSE) {
                    return FALSE;
                }
            }
            
            $ret = $this->gotData($buf);
            
            if ($ret === FALSE) //error
                break;
            if ($ret == NULL) {
                /* haven't got a full packet yet */
                if (!$wait) {
                    return NULL;
                }
            } else {
                return $ret;
            }
            
        }
        
        /* must be an error */
        return FALSE;
    }
    
    
    /** 
     * Call when data has arrived
     * @param string $in the incomming data   
     * @return array the first xml packet, or NULL if none available
     */
    function gotData($in)
    {
                
        $end = "\x0";
        
        //$this->console->writeln("dbg: $in");
        
        $this->buffer .= $in;
        
        /* just get the first packet */
        
        while (($p = strpos($this->buffer, $end)) !== FALSE) {
            $s = substr($this->buffer, 0, $p);
            $this->buffer = substr($this->buffer, $p+1);
            $this->pendingData = (strpos($this->buffer, $end) !== FALSE);
            if (ctype_digit($s)) {
                /* datalen packet - ignore it */
                continue;
            }
            return $this->parseData($s);
        }
        return NULL;
    }
    

    /**
     * Called when there's enough data to parse
     * @param string $in The incomming data
     * @return array the parsed xml, FALSE on error, NULL for nothing
     */
    private function parseData($in)
    {
        $x = new XmlToArray();
        $a = $x->parse($in);
        
        
        if (isset($a['RESPONSE'])) {
            $r = $a['RESPONSE'];
            
            //if (isset($r['_attr']['ENCODING']) && ($r['_attr']['ENCODING'] == 'base64')) {
            //    $r['_data'] = base64_decode($r['_data']);
            //}
            $hide = FALSE;
            $userCmd = $tag = '';
            
            if (isset($r['_attr']['TRANSACTION_ID'])) {
                // "pebugger-#.cmd.tag.hide
                $t = $r['_attr']['TRANSACTION_ID'];
                $p = explode('.', $t);
                
                if (isset($p[1]))
                    $userCmd = $p[1];
                if (isset($p[2]))
                    $tag = $p[2];
                if (isset($p[3]) && !empty($p[3]))
                    $hide = $p[3] != '0';
                
            }
            $this->console->hide = $hide;
            
            if ($userCmd === FALSE) {
                $userCmd = $r['_attr']['COMMAND'];
            }   
            
            if (isset($r['ERROR'])) {
                /* display the error */
                if (isset($r['ERROR']['MESSAGE'])) {
                    $s = $r['ERROR']['MESSAGE']['_data'];
                } else {
                    $s = 'Error ' . $r['ERROR']['_attr']['CODE'];
                }
                $this->console->writeln($userCmd . ': ' . $s); 
                return $r;
            }
            
            if (!isset($r['_attr']['COMMAND'])) {
                return Array();
            }
            $cmd = strtolower($r['_attr']['COMMAND']);


            /* update the status */
            if (isset($r['_attr']['STATUS'])) {
                if ($this->status != $r['_attr']['STATUS']) {
                    $this->status = $r['_attr']['STATUS'];
                    $this->console->writeln("Status set to '" . $this->status . "'"); 
                } elseif ($cmd == 'status') {
                    /* this is in response to the user's command, so display it */
                    $this->console->writeln("Status is '" . $this->status . "'"); 
                }
            }
            
            switch ($cmd) {
            case 'step_over':
            case 'step_into':
            case 'step_out':
            case 'run':
                if ($this->status == 'break') {
                    
                
                    if (($x = isset($r['XDEBUG:MESSAGE']['_attr']))) {
                        $m = $r['XDEBUG:MESSAGE']['_attr'];
                        $this->line = $m['LINENO'];
                        $this->lineOut = -1;
                        //var_dump($r['XDEBUG:MESSAGE']);
                        if ($this->filename != $m['FILENAME']) {
                            $d = relativeFile($this->filename, $m['FILENAME']);
                            $this->console->writeln($d . ':');
                            $this->filename = $m['FILENAME'];
                        }
                    }
                    
                    /* get the current frame */
                    $f = $this->cmd('stack_get', '-d 0', TRUE, '', FALSE);
                    if (!$f) {
                        return;
                    } elseif (isset($f['STACK']['_attr']['WHERE'])) {
                        if ($this->func != $f['STACK']['_attr']['WHERE']) {
                            $this->func = $f['STACK']['_attr']['WHERE'];
                            $this->console->writeln('Break in function ' . $this->func . ':');
                        }
                    }
                    
                    if ($x) {
                        $this->printSource($m['FILENAME'], $m['LINENO'], 1);
                    }
                }
                break;
            case 'detach':
                $this->console->writeln('Detached from debugger');
                return FALSE;

            case 'breakpoint_set':
                /* get the breakpoint info */
                $this->cmd('breakpoint_get', '-d ' . $r['_attr']['ID']);
                break;
                
            case 'breakpoint_get':
            case 'breakpoint_list':
            case 'breakpoint_update':
                /* add/update the breakpoint */
                $b = $r['BREAKPOINT']['_attr'];
                $bp = $this->getBreakpoint($b['ID']);
                if (($new = ($bp == NULL))) {
                    $bp = new Breakpoint($this->nextBreakpointID++);
                    $this->breakpoint[$bp->id] = $bp;
                }
                
                $bp->type = $b['TYPE'];
                $bp->enabled = $b['STATE'] == 'enabled';
                @$bp->file = $b['FILENAME'];
                @$bp->line = $b['LINENO'];
                @$bp->func = $b['FUNCTION'];
                $bp->num = $b['ID'];
                
                $this->console->writeln($bp->summary($new));
                
                break;
            case 'stack_get':
                if (!isset($r['STACK'])) {
                    $this->console->writeln("$userCmd: No stack");
                    break;
                }
                $s = $r['STACK'];

                /* make sure it's sorted, by level */
                if (isset($s[0])) {
                    $d = 0;
                    foreach ($s as $l) {
                        $stack[$l['_attr']['LEVEL']] = $l['_attr'];
                        $d++;
                    }
                } else {
                    /* only one frame */
                    $stack[0] = $s['_attr'];
                    $d = 1;
                }
                $topdir = dirname($stack[0]['FILENAME']);

                $l = strlen($d);
                $fmt = "% ${l}u: %s";
                                
                /* print it */
                $this->console->writeln('Call stack:');
                for ($i = 0; $i < $d; $i++) {
                    if ($i) {
                        $f = relativePath($topdir, dirname($stack[$i]['FILENAME']));
                        $f .= basename($stack[$i]['FILENAME']);
                    } else {
                        $f = basename($stack[$i]['FILENAME']);
                    }
                    $this->console->writeln(
                        sprintf($fmt, $i,
                            $stack[$i]['WHERE'] . " - $f:" . $stack[$i]['LINENO']
                        )
                    );
                }
                break;
            
            case 'context_get':
                if ($hide)
                    break;
                if (($verbose = (substr($tag, -1, 1) == ':'))) {
                    $context = substr($tag, 0, -1);
                } else {
                    $context = $tag;
                }
                
                if (!isset($r['PROPERTY'])) {
                    $this->console->writeln("$userCmd: No $context variables");
                    break;
                }
                $this->console->writeln(ucfirst($context) . ' variables:');
                $props = $r['PROPERTY'];
                if (!isset($props[0])) {
                    $tmp = $props;
                    $props = Array(0 => $tmp);
                }
                
                $vars = Array();
                foreach ($props as $p) {
                    $vars[] = $this->getVariableInfo($p, $verbose);
                }
                $this->console->writeln(implode(', ', $vars));
                
                break;

            }
            
            $this->console->hide = FALSE;
            
            return $r;
        
        } elseif (isset($a['INIT'])) {
            $r = $this->initData = $a['INIT'];
            if (isset($r['_attr']['IDEKEY'])) {
                if (!wildMatch(setting('idekey'), $r['_attr']['IDEKEY'])) {
                    $this->console->writeln(
                        "Connection rejected due to IDE key '" .
                        $r['_attr']['IDEKEY'] . "'"
                    );
                    return FALSE;
                }
            }
            $this->console->writeln(
                $r['ENGINE']['_data'] . ' ' . $r['ENGINE']['_attr']['VERSION'] .
                ' - ' . $r['URL']['_data'] . ' - ' . $r['COPYRIGHT']['_data']
            );
            if (isset($r['_attr']['FILEURI'])) {
                $f = $r['_attr']['FILEURI'];
                if (substr($f, 0, 7) == 'file://')
                    $f = substr($f, 7);
                $this->filename = $f;
                $this->getSource($f);
            }
            $this->cmd('step_into', '');
            return $r;
        }
        return Array();
    }
    
    /**
     * returns a string describing a variable, in response to context_get
     * @param array $p an index of $r["PROPERTY"]
     * @param bool $verbose show more info
     * @return string human-readable information about the variable
     */

    public function getVariableInfo($p, $verbose=FALSE, $depth=0)
    {
        $a = $p['_attr'];

        /* get the variable's data */
    
        switch ($a['TYPE']) {
        case 'string':
            $data = $p['_data'];
        
            if (!$verbose) {
                /* make sure it's not too long */
                $l = strlen($data);
                if ($l > 60) {
                    $data = substr($data, 0, 60);
                }
            } else
                $l = 0;
            
            /* quote it */
            $data = '"' . $data . '"';
            if ($l > 60) {
                $data .= '...';
            }
            break;
        case 'int':
            $data = $p['_data'];
            break;
        case 'array':
            if (!$verbose) {
                $v = '...';
            } elseif (isset($p['PROPERTY'])) {
                $cnt = 0;
                if (!isset($p['PROPERTY'][0])) {
                    $t = $p['PROPERTY'];
                    unset($p['PROPERTY']);
                    $p['PROPERTY'][0] = $t;
                }
                foreach ($p['PROPERTY'] as $itm) {
                    if ((++$cnt > 5) && !$verbose) {
                        $arr[] = ' ... ';
                        break;
                    }
                    if (isset($itm['_attr'])) {
                        $s = $itm['_attr']['NAME'];
                        if (!is_numeric($s)) {
                            $s = "'$s'";
                        }                        
                        $s .= ' => ';
                        $itm['_attr']['array_child'] = TRUE;
                        $s .= $this->getVariableInfo($itm, $verbose, $depth+1);
                        $arr[] = $s;
                    } else {
                        var_dump($itm);
                    }
    
                }
                $v = implode(', ', $arr);
            } else {
                $v = '';
            }
            $data = 'array(' . $v . ')';
            break;
        case 'uninitialized':
            $data = '*uninitialized*';
            break;
        case 'bool':
            $data = $p['_data'] ? 'TRUE' : 'FALSE';
            break;
        case 'null':
            $data = 'NULL';
            break;
        default:
            //var_dump($p);
            $data = '???';
        }
        
        if (isset($itm['_attr']['array_child'])) {
            $ret = '';
        } elseif (isset($a['FULLNAME'])) {
            $ret = $a['FULLNAME'] . ' = ';
        } else {
            $ret = '';
        }
        
        $ret .= $data;
        
        
        return $ret;

    }

    /**
     * get the breakpoint from the debugger's id
     */
    public function getBreakpoint($num)
    {
        foreach ($this->breakpoint as $bp) {
            if ($bp->num == $num)
                return $bp;
        }
        return NULL;
    }         
   
    /**
     * get the source for a file
     * @param string $file The file to retreive. Ommit for the current file
     * @return bool FALSE on error     
     */
    public function getSource($file = '')
    {
        if (isset($this->source[$file])) {
            if ($this->source[$file]->lineCnt > 0) {
                /* already got it */
                return TRUE;
            }
        }
        if (empty($file)) {
            $file = $this->filename;
        }
            
        $r = $this->cmd('source', "-f $file", TRUE);
        if ($r === FALSE)
            return FALSE;


        $this->source[$file] = new Source($file, $r);

        return TRUE;
    }
    
    /**
     * Display the source code
     * @param string $file the file. defaults to the current one
     * @param string $first the first line
     * @param string $num number lines
     *                    
     */    
    public function printSource($file, $first=-1, $num=1)
    {
        if (!isset($this->source[$file])) {
            $this->getSource($file);
        }
        
        if ($first > $this->source[$file]->lineCnt) {
            $first = $this->source[$file]->lineCnt - $num/2;
        }
        $last = $first + ($num - 1);

        $s = '';
        $l = strlen($last);
        $fmt = "% ${l}u:%s%s";
        $currentFile = ($this->filename == $file);
        
        for ($i = $first; $i <= $last; $i++) {
            if (isset($this->source[$file]->lines[$i])) {
                $s .= sprintf($fmt, $i, '    ', $this->source[$file]->lines[$i]);
            } else {
                $s .= sprintf($fmt, $i, '~', '');
            }
            
            if ($i < $last)
                $s .= "\n";
        }
        $this->lineOut = $i - 1;
        $this->console->writeln($s);

    }
    

    
}

?>