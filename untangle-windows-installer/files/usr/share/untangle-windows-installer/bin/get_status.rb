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

def get_num_active_hosts()
  begin
    Net::HTTP.start( "localhost", "3002" ) do |session|
      command_hash = {}
      command_hash["function"] = "get_active_hosts"
      
      request = "json_request=#{CGI.escape( command_hash.to_json )}"
      response, json_body = session.post( "/", request )
      
      unless ( response.is_a?( Net::HTTPOK ))
        $logger.warn "Invalid response: #{response}"
        $logger.warn "Body: #{json_body}"
        $result["num_active_hosts"] = -1
        return
      end
      
      body = JSON.parse( json_body )
      ## Remove all of the hosts that are not enabled.
      hosts = body["hosts"].delete_if { |host| not host["enabled"] }
      $result["num_active_hosts"] = hosts.length
    end
  rescue
    $logger.warn "Unable to connect to the ARP Eater. (#{$!})"
    $result["num_active_hosts"] = -1
  end
end

def get_memory_usage()
  memory_free = 0
  memory_total = 0

  File.open( "/proc/meminfo" ) do |f|
    f.each_line do |line|
      begin
        line_array = line.split
        case line_array[0]
        when "MemTotal:"
          memory_total = line_array[1].to_i 
        when "MemFree:"
          memory_free += line_array[1].to_i
        when "Cached:" 
          memory_free +=  line_array[1].to_i
        when "Buffers:"
          memory_free += line_array[1].to_i
        end
      rescue
        $logger.warn "Error parsing the line '#{line}'"
      end
    end
  end
  
  $result["memory"] =  {
    "total" => memory_total,
    "free"  => memory_free
  }
end

def get_network_config()
  address,netmask = `ip addr show eth0 | awk '/inet/ { print $2 ; exit }'`.strip.split( "/" )

  address = "" if address.nil?

  netmask = CIDR[netmask]
  netmask = "" if netmask.nil?
  gateway = `ip route show table all | awk '/^default/ { print $3 ; exit }'`.strip
  config_type = `awk '/^iface.*eth0/ {  print $4 ; exit } ' /etc/network/interfaces`.strip
  
  $result["network"] = {
    "config_type" => config_type,
    "address" => address,
    "netmask" => netmask,
    "gateway" => gateway
  }
end

def get_disk_usage()
  total,free = `df | awk '/\\/$/ { print $2 " " $4 ; exit }'`.strip.split
  total = -1 if total.nil?
  free = -1 if free.nil?

  total = total.to_i
  free = free.to_i

  $result["disk"] = {
    "total" => total,
    "free" => free
  }
end

def get_uptime()
  $result["uptime"] = `awk '{ print $1 }' /proc/uptime`.strip.to_f
end

## Start of script
$result = {}
$logger = Logger.new( $stderr )

get_num_active_hosts
get_memory_usage
get_network_config
get_disk_usage
get_uptime


puts $result.to_json



