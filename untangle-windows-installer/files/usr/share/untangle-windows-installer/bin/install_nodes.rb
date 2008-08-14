#!/usr/bin/rush

require "java"
require "logger"

$logger = Logger.new( STDOUT )

NoAutoStart = [ "untangle-node-openvpn" ]

def start_nodes( nodes )
  
  if ( nodes.nil? || !( nodes.is_a? Array ) || nodes.empty? )
    $logger.info( "No nodes to instantiate." )
    return
  end

  default_policy = RUSH.uvm.policyManager().getDefaultPolicy()
  node_manager = RUSH.uvm.nodeManager()
  toolbox_manager = RUSH.uvm.toolboxManager()
  
  ## This is what we need for the webui
  ## service_type = com.untangle.uvm.toolbox.MackageDesc::Type::SERVICE
  nodes.each do |name|
    begin 
      policy = default_policy
      mackage_desc = toolbox_manager.mackageDesc( name )
      policy = nil if ( !mackage_desc.isSecurity )
      tid = node_manager.instantiate( name, policy )
      node = node_manager.nodeContext( tid ).node
      node.start() unless NoAutoStart.include?( name )
    rescue
      $logger.warn( "Unable to instantiate or start #{name}, #{$!}" )
    end
  end
end

start_nodes( ENV["NODE_LIST"].split( " " ))

