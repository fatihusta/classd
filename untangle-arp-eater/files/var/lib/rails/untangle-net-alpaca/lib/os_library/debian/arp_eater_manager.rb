class OSLibrary::Debian::ArpEaterManager < OSLibrary::ArpEaterManager
  include Singleton

  Service = "/etc/init.d/untangle-arp-eater"

  SingleNICFlag = "/etc/untangle-net-alpaca/single-nic-mode"
  DefaultConfigFile = "/etc/untangle-arp-eater.conf"
  DefaultsFile = "/etc/default/untangle-arp-eater"
  NatHostsFile =  "/etc/untangle-net-alpaca/iptables-rules.d/901-single-nic-nat"
  STATUS_OK = 104
  STATUS_ERR = 99

  PostBoundary = "-----------------ae056fea542a76d4ec715"

  DefaultPort = 3002

  GetActiveHosts = { "function" => "get_active_hosts" }.to_json
  HelloWorld = { "function" => "hello_world" }.to_json
  
  def register_hooks
    os["network_manager"].register_hook( -200, "arp_eater_manager", "write_files", :hook_write_files )
    os["network_manager"].register_hook( 400, "arp_eater_manager", "run_services", :hook_run_services )
  end

  def hook_commit
    write_files
    run_services
  end

  def hook_write_files
    settings = ArpEaterSettings.find( :first )
    
    if settings.nil?
      logger.warn "No settings, writing disabled settings"
      settings = ArpEaterSettings.new( :enabled => false, :gateway => "auto", :broadcast => false )
    end
    
    if settings.enabled
      networks = ArpEaterNetworks.find( :all )
    else
      networks = [ ArpEaterNetworks.new( :enabled => false, :spoof => false, :passive => true, :gateway => "auto", :netmask => "32" ) ]
    end

    ## Serialize the settings
    file_contents = serialize_settings( settings, networks )

    ## Get the nat hosts
    nat_hosts = get_nat_hosts( settings )

    os["override_manager"].write_file( SingleNICFlag, settings.enabled, "\n" )
    
    #### serialize down to the config file.
    os["override_manager"].write_file( get_config_file, header, "\n", file_contents.to_json, "\n" )

    os["override_manager"].write_file( NatHostsFile, nat_hosts, "\n" )
  end

  def hook_run_services
    #### restart the arp eater.
    logger.warn "Unable to restart the arp eater." unless run_command( "#{Service} restart" ) == 0
  end
  
  def get_active_hosts
    ## Make the JSON request to load the hosts
    response = make_request( GetActiveHosts )

    return [] unless response["status"] == STATUS_OK

    hosts_json = response["hosts"]
    
    return [] if hosts_json.nil?

    return hosts_json.map { |h| ActiveHost.new( h["enabled"], h["address"], 
                                                h["passive"], h["gateway"] ) }
  end

  private
  def is_running
    response = make_request( HelloWorld )
    return true if response["status"] == STATUS_OK
    false
  end

  def serialize_settings( settings, networks )
    gateway = settings.gateway

    interface = settings.interface
    if ApplicationHelper.null?( interface )
      i = Interface.find( :first, :conditions => [ "wan=?", true ] )
      interface = i.os_name unless i.nil?
    end

    ## If all else fails, default to eth0.
    interface = "eth0" if ApplicationHelper.null?( interface )
    
    gateway = "0.0.0.0" if is_auto( gateway )
    settings_json = { 
      :gateway => gateway, :interface => interface,
      :enabled => settings.enabled, :broadcast => ( settings.enabled && settings.broadcast )
    }

    if ( settings.attributes.include?( "mac_addresses" ))
      mac_addresses = settings.mac_addresses
      mac_addresses = "" if mac_addresses.nil?
      mac_addresses = mac_addresses.strip
      unless (  mac_addresses.empty? )
        settings_json[:mac_addresses] = mac_addresses.split { |m| m.strip }
      end
    end

    settings_json[:networks] = []

    networks.each do |network|
      gateway = network.gateway
      
      gateway = "0.0.0.0" if is_auto( gateway )
      settings_json[:networks] << { 
        :ip => network.ip, :netmask => OSLibrary::NetworkManager.parseNetmask( network.netmask ),
        :gateway => gateway, :enabled => network.enabled,
        :spoof => network.spoof, :passive => network.passive,
        :spoof_host => network.is_spoof_host_enabled
      }
    end

    return settings_json
  end

  def make_request( request )
    begin
      post = ""
      post << "--" << PostBoundary << "\r\n"
      post << "Content-Disposition: form-data; name=\"json_request\"; filename=\"-\"\r\n"
      post << "Content-Type: application/octet-stream\r\n\r\n"
      post << request
      post << "\n\r\n--" << PostBoundary << "--\r\n"
      
      Net::HTTP.start( "localhost", get_request_port ) do |http|
        response = http.post( "/", post, 
                              { "Content-Type" => "multipart/form-data; boundary=#{PostBoundary}" } )
        return ActiveSupport::JSON.decode( response.read_body )
      end
    rescue Errno::ECONNREFUSED, ActiveSupport::JSON::ParseError
      return { "status" => STATUS_ERR }
    end
  end

  def get_config_file    
    get_defaults_value( "ARP_EATER_CONFIG_FILE", DefaultConfigFile )
  end

  def get_request_port
    get_defaults_value( "ARP_EATER_BIND_PORT", DefaultPort )
  end

  def get_defaults_value( variable, default_value )
    `[ -f "#{DefaultsFile}" ] && source #{DefaultsFile} ; echo -n ${#{variable}:-#{default_value}}`
  end

  def get_nat_hosts(settings)
    <<EOF
#!/bin/dash
 
## #{Time.new}
## Auto Generated by the Untangle Net Alpaca
## If you modify this file manually, your changes
## may be overriden
##

if [ -f /usr/share/untangle-arp-eater/bin/add_nat_host ]; then
  . /usr/share/untangle-arp-eater/bin/add_nat_host #{settings.nat_hosts}
fi

EOF
  end

  def header
    <<EOF
/* 
 * #{Time.new}
 * Auto Generated by the Untangle Net Alpaca
 * If you modify this file manually, your changes
 * may be overriden
 */
EOF
  end
end
