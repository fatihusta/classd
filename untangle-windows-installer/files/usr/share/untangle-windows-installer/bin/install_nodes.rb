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

## This is a uvm context that has a longer timeout
$remote_uvm_context = Untangle::ServiceProxy.new( "http://localhost/webui/JSON-RPC",
                                                 "RemoteUvmContext",
                                                 Untangle::RequestHandler.new( 60, 180 ))

def start_nodes( nodes )
  if ( nodes.nil? || !( nodes.is_a? Array ) || nodes.empty? )
    $logger.info( "No nodes to instantiate." )
    return
  end

  default_policy = $remote_uvm_context.policyManager().getDefaultPolicy()
  node_manager = $remote_uvm_context.nodeManager()
  toolbox_manager = $remote_uvm_context.toolboxManager()
  
  nodes.each do |name|
    begin 
      `echo "Configuring : #{name}" >| /mnt/hgfs/untangle/status || true`
    rescue
    end
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

  begin
    `echo "Installing" >| /mnt/hgfs/untangle/status || true`
  rescue
  end
end

def set_language( language_id )
  windows_map = {
    1033 => "en",
    2070 => "pt_BR",
    2052 => "zh",
    1034 => "es"
  }

  language_id = 1033 if language_id.nil?
  language_code = windows_map[language_id.to_i]
  language_code = "en" if language_code.nil?
  
  rlm = $remote_uvm_context.languageManager()
  language_list = rlm.getLanguagesList()

  ## Convert the list to a hash that uses the code as the key.
  uvm_language = nil
  language_list.each { |language| break uvm_language = language if language["code"] == language_code }
  return $logger.warn( "Unable to set language code to #{language_code}" ) if uvm_language.nil?
  
  settings = rlm.getLanguageSettings()
  settings["language"] = uvm_language["code"]
  rlm.setLanguageSettings( settings )
end

start_nodes( ENV["NODE_LIST"].split( " " ))

set_language( ENV["UVM_LANGUAGE"] )

