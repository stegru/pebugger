<?php

/**
 * @package pebugger  
 */ 
/**
 * returns $path2 relative to $path1.
 * 
 * eg; relativePath('/one/two/three/four/, '/one/two/five/six')
 * => ../../five/six 
 * both paths must be absolute, but don't have to exist 
 *  
 * @param string $path1 'current' directory
 * @param string $path2 directory to go to
 * @return string $path2 relative to $path1
 */      
function relativePath($path1, $path2)
{
    $path[0] = $path1;
    $path[1] = $path2;


    $i = 0;
    foreach ($path as &$s) {
        while (substr($s, -1) == '/')
            $s = substr($s, 0, -1); 
        $p[$i] = explode('/', $s);
        $i++;
    }
        
    while (isset($p[0][0]) && isset($p[1][0])) {
        if ($p[0][0] != $p[1][0]) {
            break;
        }
        array_shift($p[0]);
        array_shift($p[1]);
    }
    
    $ret = str_repeat('../', count($p[0]));
    return $ret . implode('/', $p[1]); 
}
function relativeFile($file1, $file2)
{
    $d = relativePath(dirname($file1), dirname($file2));
    if (substr($d, -1, 1) != '/')
        $d .= '/';
    return $d . basename($file2);
}
 

/**
 * Used to convert XML into an array
 */ 
class XmlToArray
{
    private $stack;
    private $depth;
    private $out;
    private $current;
    
    private function inElement($p, $data)
    {
        @$this->current['_data'] .= $data;
    }
    private function startElement($p, $name, $attr)
    {
        $this->stack[$this->depth++] = &$this->current;

        /* add this element to the current one */

        if (isset($this->current[$name])) {
            /* already one with the same name; create an array of them */
            if (isset($this->current[$name][0])) {
                /* already been done - just add it to the end of it */
                $this->current[$name][count($this->current[$name])] = array();
            } else {
                /* move the existing one to the first index */
                $x = $this->current[$name];
                $this->current[$name] = array($x, array());
            }
            $this->current = &$this->current[$name][count($this->current[$name]) - 1];
        } else {
            $this->current[$name] = array();
            $this->current = &$this->current[$name];
        }
        $this->current['_attr'] = $attr;
        
    }
    private function endElement($p, $name)
    {
        if (isset($this->current['_attr']['ENCODING']) && isset($this->current['_data'])) {
            if ($this->current['_attr']['ENCODING'] == 'base64') {
                $this->current['_data'] = base64_decode($this->current['_data']);
            }
        }
        $this->current = &$this->stack[$this->depth-1];
        unset($this->stack[--$this->depth]);
    }

    /**
     * Puts XML into a tree-like array
     *
     * @param string $xml The XML to parse
     * @return Array The array containing the XML data   
     */
    function parse($xml)
    {
    
        $this->depth = 0;
        $this->stack = Array();
        $this->out = Array();
        $this->current = &$this->out;
    
        $p = xml_parser_create();
        xml_set_element_handler($p, array($this, 'startElement'), array($this, 'endElement')); 
        xml_set_character_data_handler($p, array($this, 'inElement'));
        xml_parse($p, $xml);     
        xml_parser_free($p);

    
        return $this->out;
    
    }
    
}


/**
 * Checks if a string matches a wildcard pattern
 * @param string $pattern the patten to use
 * @param string $str the string to check  
 * @return bool TRUE if there's a match 
 */
function wildMatch($pattern, $str)
{
    if (function_exists('fnmatch')) {
        return fnmatch($pattern, $str);
    }
    
    /* convert the wildcard pattern into a regex */
    $p = '^' . strtr($pattern, Array('*' => '.*', '?' => '.')) . '$';
    
    return eregi($p, $str) ? TRUE : FALSE;
    
}

?>