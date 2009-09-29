#!/usr/bin/env ruby

## Copyright (c) 2003-2008 Untangle, Inc.
##  All rights reserved.
## 
##  This software is the confidential and proprietary information of
##  Untangle, Inc. ("Confidential Information"). You shall
##  not disclose such Confidential Information.
## 
##  $Id: ADConnectorImpl.java 15443 2008-03-24 22:53:16Z amread $
## 

require "json"
require "net/http"
require "cgi"
require "logger"

CIDR = {
  "0"  => "0.0.0.0",
  "1"  => "128.0.0.0",
  "2"  => "192.0.0.0",
  "3"  => "224.0.0.0",
  "4"  => "240.0.0.0",
  "5"  => "248.0.0.0",
  "6"  => "252.0.0.0",
  "7"  => "254.0.0.0",
  "8"  => "255.0.0.0",
  "9"  => "255.128.0.0",
  "10" => "255.192.0.0",
  "11" => "255.224.0.0",
  "12" => "255.240.0.0",
  "13" => "255.248.0.0",
  "14" => "255.252.0.0",
  "15" => "255.254.0.0",
  "16" => "255.255.0.0",
  "17" => "255.255.128.0",
  "18" => "255.255.192.0",
  "19" => "255.255.224.0",
  "20" => "255.255.240.0",
  "21" => "255.255.248.0",
  "22" => "255.255.252.0",
  "23" => "255.255.254.0",
  "24" => "255.255.255.0",
  "25" => "255.255.255.128",
  "26" => "255.255.255.192",
  "27" => "255.255.255.224",
  "28" => "255.255.255.240",
  "29" => "255.255.255.248",
  "30" => "255.255.255.252",
  "31" => "255.255.255.254",
  "32" => "255.255.255.255"
}

def get_active_hosts()
  $result["active_hosts"] = nil
  
  begin
    Net::HTTP.start( "localhost", "3002" ) do |session|
      command_hash = {}
      command_hash["function"] = "get_active_hosts"
      
      request = "json_request=#{CGI.escape( command_hash.to_json )}"
      response, json_body = session.post( "/", request )
      
      unless ( response.is_a?( Net::HTTPOK ))
        $logger.warn "Invalid response: #{response}"
        $logger.warn "Body: #{json_body}"
        return
      end
      
      body = JSON.parse( json_body )
      ## Remove all of the hosts that are not enabled.
      $result["active_hosts"] = body["hosts"]
    end
  rescue
    $logger.warn "Unable to connect to the ARP Eater. (#{$!})"
    
  end
end

## Start of script
$result = {}
$logger = Logger.new( $stderr )

get_active_hosts

puts $result.to_json

