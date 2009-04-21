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

require "ftools"
require "logger"

def get_properties
  properties = {}

  `VBoxControl -nologo guestproperty enumerate -patterns "#{PropertyPrefix}*"`.each_line do |line|
    match = PropertyRegex.match( line )
    $logger.debug( "Testing #{line} for #{PropertyRegex}" )
    next if match.nil?

    ## Wakeup is used to tell the action handler to ... wakeup.
    property_name = match[1]
    next if ( property_name == "#{PropertyPrefix}wakeup" )
    
    ## Ignoring the timestamp
    properties[property_name] = match[2]
  end

  properties
end

def handle_actions
  ## Just wait for a change (all of the parameters are enumerated in parse_properties).
  Kernel.system( "VBoxControl -nologo guestproperty wait '#{PropertyPrefix}*' > /dev/null 2>&1" )

  properties = get_properties

  return if properties.nil?

  properties.each do |name,value|
    ## Remove the property
    Kernel.system( "VBoxControl", "-nologo",  "guestproperty", "set", name )

    output_prefix = name.sub( PropertyPrefix, "#{OutputDirectory}" )
    if ( name == output_prefix )
      $logger.warn "Invalid key #{name}"
      next
    end

    ## Should add a command timeout.
    $logger.debug( "Running the command #{value} > #{output_prefix}.out 2>#{output_prefix}.err" )
    Kernel.system( "#{value} > #{output_prefix}.out 2>#{output_prefix}.err" )

    ret_key = name.sub( PropertyPrefix, "#{ReturnPrefix}" )

    ## Exit is used to tell the action handler to ... exit
    $is_running = false if ( name == "#{PropertyPrefix}exit" )

    Kernel.system( "VBoxControl", "-nologo", "guestproperty", "set", "#{ret_key}.ret", $?.exitstatus.to_s )
  end
end

SharedDirectory = "/mnt/hgfs"
poll_timeout = ENV["POLL_TIMEOUT"]
PollTimeout = poll_timeout.nil? ? 1.0 : poll_timeout

PropertyPrefix = "/Untangle/Host/Command/"
ReturnPrefix = "/Untangle/Host/ReturnCode/"

OutputDirectory = "/mnt/hgfs/logs/"
PropertyRegex = /Name: (.*), value: (.*), timestamp: (.*), flags: .*/

$is_running = true
$logger = Logger.new(STDERR)
$logger.level = Logger::INFO

Kernel.system( "mkdir", "-p", OutputDirectory )
while $is_running
  begin
    sleep PollTimeout
    handle_actions
  rescue
    sleep( PollTimeout * 2 )
  end
end
