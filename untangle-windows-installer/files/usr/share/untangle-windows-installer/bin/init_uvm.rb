#!/usr/bin/env ruby

require "logger"
require "json"

$logger = Logger.new( STDOUT )

## All of the pacakges should already be inside of the cache.
INST_OPTS = " -o DPkg::Options::=--force-confnew --yes --force-yes --fix-broken --purge --no-download "

def install_packages( config )
  packages = config["packages"]

  if ( packages.nil? || !( packages.is_a? Array ) || packages.empty? )
    $logger.info( "No packages to install." )
    return
  end

  Kernel.system( "apt-get install #{INST_OPTS} #{packages.join( " " )}" )
end

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
  install_nodes = "usr/share/untangle-windows-installer/bin/install_nodes.rb"

  Kernel.system( "#{rush_shell} #{install_nodes}" )
end

## Parse the config file
config_file = ARGV[0]
config = ""
File.open( config_file, "r" ) { |f| f.each_line { |l| config << l }}
config = ::JSON.parse( config )

install_packages( config )

start_nodes( config )
