class Alpaca::Components::ArpEaterComponent < Alpaca::Component
  def register_menu_items( menu_organizer, config_level )
    if ( config_level >= AlpacaSettings::Level::Advanced ) 
      menu_organizer.register_item( "/main/advanced/snic", menu_item( 1000, "Single NIC", :action => "manage" ))
    end
  end
end
