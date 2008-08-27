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

require "logger"
require "json"

$logger = Logger.new( STDOUT )

def start_nodes( config )
  nodes = config["nodes"]

  if ( nodes.nil? || !( nodes.is_a? Array ) || nodes.empty? )
    $logger.info( "No nodes to start." )
    return
  end

  ENV["NODE_LIST"] = nodes.join( " " )

  rush_shell=ENV["RUSH_SHELL"]
  rush_shell="/usr/bin/rush" if rush_shell.nil?

  install_nodes = ENV["INSTALL_NODES"]
  install_nodes = "/usr/share/untangle-windows-installer/bin/install_nodes.rb"

  $logger.info( "#{rush_shell} #{install_nodes}" )
  Kernel.system( "#{rush_shell} #{install_nodes} 2>&1" )
end

## Parse the config file
config_file = ARGV[0]
config = ""
File.open( config_file, "r" ) { |f| f.each_line { |l| config << l }}
config = ::JSON.parse( config )

start_nodes( config )
