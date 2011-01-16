<?php

/**
 * A very basic replacement for the readline extension
 * Check if readline is not available before including this file!
 * 
 * @package pebugger
 */  



$rl_callback = NULL;
$rl_prompt = '';
$rl_history = NULL;
$rl_buffer = '';
$rl_term = NULL;

function rl_displayPrompt()
{
    global $rl_prompt, $rl_buffer;
    echo($rl_prompt . $rl_buffer);
}
function rl_shutdown()
{
    global $rl_term;
    if ($rl_term != NULL) {
        /* put the terminal back the way it was */
        exec("stty '$rl_term'");
    }
}

function readline_callback_handler_install($prompt, $callback)
{
    global $rl_callback, $rl_prompt, $rl_term;
    
    $rl_prompt = $prompt;
    $rl_callback = $callback;


    /* try to turn off the line buffering */

    /* save the current terminal settings */
    $rl_term = shell_exec('stty --save 2>/dev/null');
    if ($rl_term != NULL) {
        exec('stty -icanon -echo');
    }

}

function readline_on_new_line()
{
}

function readline_redisplay()
{
    rl_displayPrompt();
}
function readline_add_history($line)
{
    global $rl_history;
    $rl_history[] = $line;
}

function readline_callback_read_char()
{
    global $rl_buffer, $rl_callback, $rl_prompt, $rl_term, $stdin;

    if (feof($stdin)) {
        exit();
    }
    
    $ch = fgetc($stdin);
    
    switch ($ch) {
    case "\n":
        echo("\n");
        call_user_func($rl_callback, $rl_buffer);
        $rl_buffer = '';
        rl_displayPrompt();
        break;
        
    /* backspace */
    case "\x08": // ^H
    case "\x7F": // ^?
        if ($rl_term != NULL) {
            /* clear the line (portable) */
            echo("\r" . str_repeat(' ', strlen($rl_buffer)+strlen($rl_prompt)) . "\r");
            /* lose the last char */
            $rl_buffer = substr($rl_buffer, 0, -1);
            /* re-display */
            rl_displayPrompt();
        }
        break;
    default:
        if (ctype_print($ch)) {
            echo($ch);
            $rl_buffer .= $ch;
        }
    }
}

function readline_completion_function($func)
{
    /* not implemented */
}

function readline_info($varname = '', $newvalue = '')
{
    global $rl_buffer;
    
    switch ($varname) {
    case 'line_buffer':
        return $rl_buffer;
    case 'end':
        return strlen($rl_buffer);
    }
}

?>
