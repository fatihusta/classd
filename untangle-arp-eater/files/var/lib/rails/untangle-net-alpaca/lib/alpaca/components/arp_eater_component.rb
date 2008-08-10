class Alpaca::Components::ArpEaterComponent < Alpaca::Component
  def register_menu_items( menu_organizer, config_level )
    if ( config_level >= AlpacaSettings::Level::Advanced ) 
      menu_organizer.register_item( "/main/advanced/snic", menu_item( 1000, "Single NIC", :action => "manage" ))
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
                                    :passive => false, :gateway => "auto", 
                                    :description => "Local network" )

    if ( config["single_nic_mode"] == true )
      settings.enabled = true
      network.enabled = true
      network.spoof = true
    end

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
