## Copyright (c) 2003-2008 Untangle, Inc.
##  All rights reserved.
## 
##  This software is the confidential and proprietary information of
##  Untangle, Inc. ("Confidential Information"). You shall
##  not disclose such Confidential Information.
## 
##  $Id: install_nodes.rb 15443 2008-03-24 22:53:16Z rbscott $
## 

require "logger"

$logger = Logger.new( STDOUT )

NoAutoStart = [ "untangle-node-openvpn" ]

def start_nodes( nodes )
  if ( nodes.nil? || !( nodes.is_a? Array ) || nodes.empty? )
    $logger.info( "No nodes to instantiate." )
    return
  end

  default_policy = Untangle::RemoteUvmContext.policyManager().getDefaultPolicy()
  node_manager = Untangle::RemoteUvmContext.nodeManager()
  toolbox_manager = Untangle::RemoteUvmContext.toolboxManager()
  
  nodes.each do |name|
    begin 
      $logger.info( "Attempting to instantiate #{name}." )

      policy = default_policy
      mackage_desc = toolbox_manager.mackageDesc( name )
      policy = nil if ( mackage_desc["type"] == "SERVICE" )
      nodeDesc = node_manager.instantiate( name, policy )

      if nodeDesc.nil?
	$logger.warn "instantiate didn't return a nodeDesc"
	next
      end

      node = node_manager.nodeContext( nodeDesc['tid'] ).node
      node.start() unless NoAutoStart.include?( name )
    rescue
      $logger.warn( "Unable to instantiate or start #{name}, #{$!}" )
    end
  end
end

start_nodes( ENV["NODE_LIST"].split( " " ))

