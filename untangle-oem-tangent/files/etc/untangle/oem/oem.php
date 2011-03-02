<?php

$oem_name="WebHawk";
$oem_url="http://w3hawk.com";

function seattext(){
  $text  = '<option value="50">1-50</option>';
  $text .= '<option value="150">51-150</option>';
  $text .= '<option value="300">151-300</option>';
  $text .= '<option value="500">301-500</option>';
  $text .= '<option value="750">501-750</option>';
  $text .= '<option value="1000">751-1000</option>';
  $text .= '<option value="1500">1001-1500</option>';
  $text .= '<option value="5000">1501+</option>';
  return $text;
}

?>
