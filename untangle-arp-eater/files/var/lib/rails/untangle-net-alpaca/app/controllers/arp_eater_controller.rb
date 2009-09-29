class ArpEaterController < ApplicationController
  def get_settings
    settings = {}

    networks = ArpEaterNetworks.find( :all )
    if networks.empty?
      networks << ArpEaterNetworks.new( :enabled => false, :spoof => true, :passive => true, 
                                        :ip => "0.0.0.0", :netmask => "0", :gateway => "auto",
                                        :description => "local network" )
    end
    settings["networks"] = networks
    
    active_hosts = os["arp_eater_manager"].get_active_hosts
    active_hosts = active_hosts.sort_by { |i| IPAddr.parse( i.address ).to_i }
    settings["active_hosts"] = active_hosts
    
    arp_eater_settings = ArpEaterSettings.find( :first )
    if arp_eater_settings.nil?
      arp_eater_settings = ArpEaterSettings.new( :enabled => false, :gateway => "auto", :broadcast => false )
    end
    settings["arp_eater_settings"] = arp_eater_settings

    json_result( :values => settings )
  end
  
  def set_settings
    s = json_params
    
    settings = ArpEaterSettings.find( :first )
    settings = ArpEaterSettings.new if settings.nil?
    settings.update_attributes( s["arp_eater_settings"] )
    settings.save

    networks = s["networks"]
    ArpEaterNetworks.destroy_all
    s["networks"].each do |network| 
       ArpEaterNetworks.new( network ).save
    end

    os["arp_eater_manager"].commit

    json_result
  end

  def get_active_hosts
    active_hosts = os["arp_eater_manager"].get_active_hosts
    active_hosts = active_hosts.sort_by { |i| IPAddr.parse( i.address ).to_i }
    json_result( :values => active_hosts )
  end

  alias_method :index, :extjs
end
