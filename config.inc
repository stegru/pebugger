<?php


/**
 * Contains anything to do with loading/saving configuration options
 * 
 * @package pebugger  
 */ 



/**
 * Contains information about a setting
 */ 
class Setting
{
    public $name = '';
    public $value = NULL;
    public $help = '';
    public $type = 'string';
    public $default;
    public $builtin = FALSE;
    
    public function __construct($name, $value = NULL, $help = '')
    {
        $this->default = $this->value = $value;
        $this->name = $name;
        $this->help = $help;
    }
    public function getHelp($full = FALSE)
    {
        if (empty($this->help)) {
            $s = '[no help available]';
        } else {
            $s = $this->help;
        }
        if ($full) {
            return $this->name . ": $s\nDefault value: " . $this->default;
        } else {
            /* just show the first line */
            $a = split("\n", $s, 2);
            return $a[0];
        }
    }
    
}

/* default settings */
$SETTINGS = Array(
    'prompt' => new Setting('', '->', 'The default prompt'),
    'browser' => new Setting('', 'lynx -dump %s', "The command to start a browser.\nUse %s to insert the url. The command must not require any interaction from stdin, and any output is ignored."),
    'port' => new Setting('', 9000, 'The port to listen on.'),
    'bind' => new Setting('', '0.0.0.0', 'The address to listen on. Use 0.0.0.0 for any.'),
    'accept' => new Setting('', '*', "Only accept debug sessions from hosts that match this value.\nThis is matched against both the hostname and IP address of the incomming connection"),
    'idekey' => new Setting('', '*', 'Only accept debug sessions with an IDE key that matches this value.'),
);
foreach ($SETTINGS as $n => $s) {
    $s->name = $n;
    $s->builtin = TRUE;
}


/**
 * Returns a configuration setting
 * It's just a wrapper (and prefered interface) for the $SETTINGS[] global.
 * @param string $setting the setting to return
 * @param string $default the default value; returned if the setting isn't set  
 * @return mixed the setting
 */
function setting($setting, $default = '')
{
    global $SETTINGS;
    if (!isset($SETTINGS[$setting])) {
        return $default;
    }
    return $SETTINGS[$setting]->value;
}

/**
 * sets a setting
 * @param string $setting the setting to set
 * @parem mixed $value the setting's value
 */  
function setSetting($setting, $value=NULL)
{
    global $SETTINGS;
    if (!isset($SETTINGS[$setting])) {
        $SETTINGS[$setting] = new Setting($setting, $value);
    } else {
        $SETTINGS[$setting]->value = $value;
    }
}

/**
 * reads the users configuration file
 */ 
function readConfig()
{
    global $SETTINGS;
    
    $d = getUserDir(FALSE);
    if ($d !== FALSE) {
        $c = readConfigFile($d . '/config');
        foreach ($c as $k => $v) {
            setSetting($k, $v);
        }
        //$SETTINGS = array_merge($SETTINGS, $c);
    }
}

/**
 * Saves the user's configuration
 */
function saveConfig()
{
    global $SETTINGS;
    $d = getUserDir(TRUE);
    if ($d !== FALSE) {
        $a = Array();
        foreach ($SETTINGS as $s) {
            $a[$s->name] = $s->value;
        }
        saveConfigFile($a, $d . '/config');
    }
    
}

/**
 * Reads in the options from a config file.
 * The file (created with saveConfig()), is PHP code and any variables defined
 * will be read into the returned array.
 * @param string $file the file to read from 
 * @return array the options as name => value 
 * The file, written by save
 */ 
function readConfigFile($file)
{
    /* execute the config file, and stash any variables it has defined */
    @include($file);
    $vars = get_defined_vars();
    
    $a = Array();
    foreach ($vars as $n => $v) {
        $a[$n] = $v;
    }
    return $a;
}


/**
 * Saves settings to a file
 * The file will be overwritten with valid PHP code that defines the variables
 * specified in $conf. Intended to be read back in with readConfig().
 *  
 * @param array $conf The array of settings; 'name' => 'value' 
 * @param string $file The file to write to
 * @return FALSE on error  
 *  
 */ 
function saveConfigFile($conf, $file)
{
    
    $f = fopen($file, 'w');
    if ($f === FALSE) {
        return FALSE;
    }
    
    fputs($f, "<?php\n# This file is auto-generated by pebugger.\n# Try not to edit yourself - any formatting and comments will be lost!\n\n");
    $out = '';
    foreach ($conf as $n => $v) {
        $out .= "$$n = " . var_export($v, TRUE) . ";\n";
    }
    fputs($f, $out . "\n\n?>\n");
    
    fclose($f);
}

/**
 * Returns the directory where user configs are put; ~/.pebugger/
 * Depends on the $HOME environment variable
 * @param bool $create if TRUE, create the directory
 * @return string a directory that contains the user's config files
 */ 
function getUserDir($create = TRUE)
{
    $home = getenv('HOME');
    if (!isset($home) || ($home === FALSE)) {
        return FALSE;
    }
    $d = "$home/.pebugger";
    if (!is_dir($d)) {
        if ($create) {
            mkdir($d);
        } else {
            return FALSE;
        }
    }
    return $d;
}

?>