class Alpaca::Components::ArpEaterComponent < Alpaca::Component
  def register_menu_items( menu_organizer, config_level )
    if ( config_level >= AlpacaSettings::Level::Advanced )
      ## Only show the single NIC menu if there is just one NIC.
      if ( Interface.count( :conditions => [ "os_name != ?", Interface::Unmapped ] ) == 1 )
        menu_organizer.register_item( "/advanced/snic", menu_item( 1000, "Re-Router", :action => "index" ))
      end
    end
  end

  class Preconfiguration
    def initialize( settings, networks )
      @settings, @networks = settings, networks
    end

    attr_reader :settings, :networks
  end

  def pre_prepare_configuration( config, settings_hash )
    settings = ArpEaterSettings.new( :enabled => false, :gateway => "auto", :broadcast => false ) 
    network = ArpEaterNetworks.new( :enabled => false, :spoof => false, :ip => "0.0.0.0", :netmask => "0",
                                    :passive => true, :gateway => "auto", 
                                    :description => "Local network" )

    if ( config["single_nic_mode"] == true )
      settings.enabled = true
      settings.broadcast = true
      network.enabled = true
      network.spoof = true
    end

    mac_addresses = config["mac_addresses"] or ""
    mac_addresses = mac_addresses.strip
    
    settings.mac_addresses = mac_addresses

    settings_hash[self.class] = Preconfiguration.new( settings, [ network ] )
  end

  def pre_save_configuration( config, settings_hash )
    ArpEaterSettings.destroy_all
    ArpEaterNetworks.destroy_all

    s = settings_hash[self.class]
    
    s.settings.save
    s.networks.each { |n| n.save }
  end
end
