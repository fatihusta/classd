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
                                                 Untangle::RequestHandler.new( 150, 240 ))

def update_status( status, node_name = nil )
  if node_name.nil?
    message = status
  else
    message = "#{status}\n#{node_name}"
  end
  begin
    `/bin/echo -e "#{message}" >| /mnt/hgfs/untangle/status || true`
  rescue
    $logger.warn( "Unable to update status #{$!}" )
  end
end

def add_warning( message )
  begin
    `/bin/echo -e "#{message}" >| /mnt/hgfs/untangle/install.warn || true`
  rescue
    $logger.warn( "Unable to update warning #{$!}" )
  end  
end

def start_nodes( nodes )
  if ( nodes.nil? || !( nodes.is_a? Array ) || nodes.empty? )
    $logger.info( "No nodes to instantiate." )
    return
  end

  default_policy = $remote_uvm_context.policyManager().getDefaultPolicy()
  node_manager = $remote_uvm_context.nodeManager()
  toolbox_manager = $remote_uvm_context.toolboxManager()
  
  nodes.each do |name|
    update_status( "Configuring", name )

    begin 
      $logger.info( "Attempting to instantiate #{name}." )

      policy = default_policy
      mackage_desc = toolbox_manager.mackageDesc( name )
      policy = nil if ( mackage_desc["type"] == "SERVICE" )
      nodeDesc = node_manager.instantiate( name, policy )

      if nodeDesc.nil?
	$logger.warn( "instantiate didn't return a nodeDesc" )
	next
      end

      node = node_manager.nodeContext( nodeDesc['tid'] ).node
      node.start() unless NoAutoStart.include?( name )
    rescue Timeout::Error
      $logger.warn( "Timeout trying to start #{name}, #{$!}" )
      add_warning( "config failed [timeout] #{name}" )

    rescue 
      $logger.warn( "Unable to instantiate #{name}, #{$!}" )
      add_warning( "config failed [#{$!}] #{name}" )
    end
  end

  update_status( "Configuring" )
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

node_list = ENV["NODE_LIST"].split( " " )

## Enable webfilter last, it uses a lot of CPU at startup and this
## way we incur that hit after all of the other nodes have been installed.
if ( node_list.include?( "untangle-node-webfilter" ))
  node_list.delete( "untangle-node-webfilter" )
  node_list << "untangle-node-webfilter"
end

def update_registration_info()
  am = $remote_uvm_context.adminManager()
  registration_info =  am.getRegistrationInfo()
  am.setRegistrationInfo(registration_info)
end

### Start of script
begin
  set_language( ENV["UVM_LANGUAGE"] )
rescue Timeout::Error
  $logger.warn( "Unable to set language #{$!}" )
rescue
  $logger.warn( "Unable to set language #{$!}" )
end

begin
  update_registration_info()
rescue Timeout::Error
  $logger.warn( "Timeout trying to update registration info #{$!}" )
  
rescue
  $logger.warn( "Unable to update registration info #{$!}" )
end

## Do this at the end, the nodes take a lot of CPU.
start_nodes( node_list )
