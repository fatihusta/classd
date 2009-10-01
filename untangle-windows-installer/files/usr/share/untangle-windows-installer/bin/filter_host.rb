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

require "#{File.dirname(__FILE__)}/add_host_ips.rb"

def filter_host( ip_addresses, mac_addresses, overwrite_arp_eater_settings )
  ip_addresses = "" if ip_addresses.nil?
  
  ip_addresses = ip_addresses.split( "," ).map { |ip_address| ip_address.strip }
  
  ## Delete the invalid entries
  ip_addresses.delete_if do |ip_address|
    begin
      IPAddr.new( ip_address )
      next false
    rescue
      $logger.warn( "Ignorning the address #{ip_address}" )
      true
    end
  end

  mac_addresses = "" if mac_addresses.nil?
  mac_addresses = mac_addresses.split( "," ).map { |mac_address| mac_address.strip }

  begin
    Net::HTTP.start( "localhost", "3000" ) do |session|
      response, json_body = session.get( "/alpaca/arp_eater/get_settings?argyle=#{$nonce}" )
      
      unless ( response.is_a?( Net::HTTPOK ))
        $logger.warn "Invalid response: #{response}"
        $logger.warn "Body: #{json_body}"
        return
      end
      
      body = JSON.parse( json_body )
      headers = { "Content-type" => "application/json" }
      settings = body["result"]
      networks = settings["networks"]

      ## Delete all of the previous ip addresses
      networks.delete_if { |network| network["description"] == HOST_IP_MARKER }

      ip_addresses.each do |ip_address|
        ip_address = ip_address
        networks.insert( 0, { "netmask" => "32", "enabled" => true, "ip" => ip_address,
                           "gateway" => "auto", "spoof" => true, "passive" => true,
                           "is_spoof_host_enabled" => false, "description" => HOST_IP_MARKER } )
      end
      
      if overwrite_arp_eater_settings
        settings["arp_eater_settings"]["mac_addresses"] = mac_addresses.join( " " )
        settings["arp_eater_settings"]["nat_hosts"] = ip_addresses.join( " " )
      end

      response, json_body = session.post( "/alpaca/arp_eater/set_settings?argyle=#{$nonce}", 
                                          settings.to_json, headers )

      unless ( response.is_a?( Net::HTTPOK ))
        $logger.warn "Invalid response: #{response}"
        $logger.warn "Body: #{json_body}"
        return
      end

      response, json_body = session.get( "/alpaca/uvm/get_settings?argyle=#{$nonce}" )

      unless ( response.is_a?( Net::HTTPOK ))
        $logger.warn "Invalid response: #{response}"
        $logger.warn "Body: #{json_body}"
        return
      end

      body = JSON.parse( json_body )
      settings = body["result"]
      subscriptions = settings["user_subscriptions"]
      subscriptions.delete_if { |subscription| subscription["description"] == HOST_IP_MARKER }
      
      response, json_body = session.post( "/alpaca/uvm/set_settings?argyle=#{$nonce}", 
                                          settings.to_json, headers )

      unless ( response.is_a?( Net::HTTPOK ))
        $logger.warn "Invalid response: #{response}"
        $logger.warn "Body: #{json_body}"
        return
      end
    end
  rescue Timeout::Error
    $logger.warn( "Timeout while trying to talk to the alpaca" )
  rescue
    $logger.warn( "Unknown exception while talking to the alpaca\n#{$!}" )
  end
end

$logger = Logger.new( $stderr )
$nonce = get_nonce( "/etc/untangle-net-alpaca/nonce" )

case ARGV[2]
when "true"
  filter_host( ARGV[0], ARGV[1], true )

when "false"
  add_host_ips( ARGV[0], ARGV[1] )
  
when "disabled"
  filter_host( ARGV[0], ARGV[1], false )

else
  add_host_ips( ARGV[0], ARGV[1] )
end


