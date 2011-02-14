<?php
session_start();
?>
  <link rel="stylesheet" type="text/css" href="oem.css" />
<?
include ("/etc/untangle/oem/oem.php");

function in_multiarray($elem, $array) 
{ 
  // if the $array is an array or is an object 
  if( is_array( $array ) || is_object( $array ) ) 
  { 
    // if $elem is in $array object 
    if( is_object( $array ) ) 
    { 
      $temp_array = get_object_vars( $array ); 
      if( in_array( $elem, $temp_array ) ) 
        return TRUE; 
    } 

    // if $elem is in $array return true 
    if( is_array( $array ) && in_array( $elem, $array ) ) 
      return TRUE; 


    // if $elem isn't in $array, then check foreach element 
    foreach( $array as $array_element ) 
    { 
      // if $array_element is an array or is an object call the in_multiarray function to this element 
      // if in_multiarray returns TRUE, than return is in array, else check next element 
      if( ( is_array( $array_element ) || is_object( $array_element ) ) && in_multiarray( $elem, $array_element ) ) 
      { 
        return TRUE; 
        exit; 
      } 
    } 
  } 

  // if isn't in array return FALSE 
  return FALSE; 
}
    
/**
 * storeAPI function
 *
 * @param $action Action to be performed
 * @param $postData array of data to act upon
 * @return void
 * @author Marc Runkel
 **/

function storeAPI($storeAction,$postData = array())
{
  $ch = curl_init();
  $strCookie = session_name()."=".session_id()."; path=".session_save_path();
  curl_setopt($ch, CURLOPT_URL, "http://staging-store.untangle.com/untangle_admin/oem/oemapi.php");
  curl_setopt($ch, CURLOPT_RETURNTRANSFER, 1);
  curl_setopt($ch, CURLOPT_COOKIE, $strCookie );
  $postData['action'] = $storeAction;
  curl_setopt($ch, CURLOPT_POSTFIELDS, $postData);
  $response = curl_exec($ch);
  if ($response == FALSE) {
    curl_close($ch);
    return array('result' => "Invalid");
  } else {
    curl_close($ch);
    return (array) json_decode($response, true);
  }
}

function install($term, $seats, $install)
{
  $itemlist = array();
  array_multisort($install, SORT_ASC);    // sort the inbound array so that the PAIDs are first so we can cull the TRIALs
  foreach ($install as $type => $list) {
    foreach ($list as $sku => $value) {
      if (!in_multiarray($sku, $itemlist)) {  // don't add a trial if we're installing a paid app..   
        $itemlist[] = array ('SKU' => $sku, 'type' => $type);
      }
     }
  }
  $vars = array ('term' => $term,
                 'usercount' => $seats,
                 'list' => json_encode($itemlist));
  $actions = storeAPI ('install', $vars);
  echo "<BR/>Installing ...<BR/>";
  foreach ($actions as $action) {
    if ($action['status'] == 'Valid') {
      $urls = "";
      foreach (explode(',',$action['libitems']) as $libitem) {
        $urls .= "<img src=\"/library/install.png?libitem=" . $libitem . "\"> ";
      }
      echo "<br><img src=\"" . $action['thumbnail'] . "\">" . $action['name'] . $urls;
    } else {
      echo "<BR>System Error Occured, please call Support.<BR>" . $action['message'];
    }
  }?>
  <BR><BR><BR>Installation is complete.  You may now close this tab.<BR>
  <?
}

function getProducts()
{
  return storeAPI("querysku");
}

if ($_SESSION['uid'] == "") { // read the UID from the box and store it in the session variable
  if (!$uid_rsc = fopen('/usr/share/untangle/conf/uid', 'r')) {
    echo "UID not created yet.";
    exit;
  }
  $_SESSION['uid'] = fread($uid_rsc, 32);
}

$store_result = array();
if ($_REQUEST['action'] == "login") {
  $vars = array ('email' => $_REQUEST['email'],
                  'password' => $_REQUEST['password'],
                  'uid' => $_SESSION['uid']);
  $store_result = storeAPI("login",$vars);
  if ($store_result['result'] == "Valid") {
    $_SESSION['login'] = $store_result['UserID'];
  } else {  //login failed
    $msg = "<strong> Login Failed </strong><br />";
  }
}

if ($_SESSION['login'] == "") { // not logged in? display login screen.
  ?>
<form method="post">   
<fieldset>   
<legend>OEM Store Login</legend>   
<ol>   
<li>   
<label for="email">Email address:<em>REQUIRED</em></label>   
<input id="email" name="email" class="text" type="text" width="40" />   
</li>   
<li>   
<label for="password">Password:<em>REQUIRED</em><? echo $msg ?></label>   
<input id="password" name="password" class="password" type="password" />   
</li>   
</ol>
<input id="action" name="action" value="login" type="hidden" />   
</fieldset>
<fieldset class="submit">   
<input class="submit" type="submit"   
value="Login" />   
</fieldset>  
</form>

  <? exit;   // and terminate
}   // otherwise, process the action.
switch ($_REQUEST['action']) {
  case 'install':
    install($_REQUEST['term'],$_REQUEST['seats'],$_REQUEST['install']);
    break;
  case 'logout':
    $_SESSION['login'] = "";
    echo "<BR>Logged out -- Click <a href=\"/oem\">here</a> to login again.";
    break;
  default:  // no action, display first menu
    $products = getProducts();    // get the possible SKUs to display
    echo "<!--";
    print_r ($products);
    echo "-->";
    
  ?>
    <form id="logout" name="logout" method="post"><input id="action" name="action" value="logout" type="hidden" /></form>
    <form id="install" name="install" method="post">
      <fieldset>
      <legend>Choose Products for installation/trial</legend>

      <table>
      <tr><th class="product">Product</th><th class="checkbox">Full</th><th class="checkbox">Trial</th></tr>
<?
    foreach ($products as $product) {
      echo "<tr>\n";
      echo "<td class=\"product\">" . $product['Name'] . "</td>\n";
      echo "<td class=\"checkbox\"><input type=\"checkbox\" name=\"install[PAID][" . $product['SKU'] . "]\" value=\"yes\" id=\"" . $product['SKU'] . "_paid\" /></td>\n";
      echo "<td class=\"checkbox\"><input type=\"checkbox\" name=\"install[TRIAL][" . $product['SKU'] . "]\" value=\"yes\" id=\"" . $product['SKU'] . "_trial\" /></td>\n";
      echo "</tr>\n";
    }
?>

      </table></fieldset>
      <fieldset>
      <legend>Chose Term and Seat Count</legend>
      <ol>
      <li>
        <label for="term">Term: </label>
        <select name="term" id="term" class="select">
          <option value="1">Monthly</option>
          <option value="12">Annual</option>
          <option value="24">Two Year</option>
          <option value="36">Three Year</option>
        </select>
        </li>
        <li>
          <label for="seats">Seats: </label>
          <select name="seats" id="seats" class="select">
            <option value="10">1-10</option>
            <option value="50">11-50</option>
            <option value="150">51-150</option>
            <option value="1500">151-1500</option>
            <option value="3000">1501+</option>
          </select>
      </li>
      </ol>
      </fieldset>
      <input id="action" name="action" value="install" type="hidden" />   
      <a class="button" href="#" onclick="this.blur(); document.logout.submit();"><span>Logout</span></a> <a class="button" href="#" onclick="this.blur(); document.install.submit();"><span>Install</span></a>
    </form>  
<?
    
    break;
}


?>