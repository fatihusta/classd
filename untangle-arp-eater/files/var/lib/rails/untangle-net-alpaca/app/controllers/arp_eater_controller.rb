class ArpEaterController < ApplicationController
  def manage
    @networks = ArpEaterNetworks.find( :all )
    @settings = ArpEaterSettings.find( :first )
    @active_hosts = os["arp_eater_manager"].get_active_hosts
    
    @settings = ArpEaterSettings.new( :enabled => false, :gateway => "auto", :broadcast => false ) if @settings.nil?
    render :action => 'manage'
  end

  def create_network
    @network = ArpEaterNetworks.new( :enabled => true, :spoof => true,
                                     :opportunistic => false, :gateway => "auto" )

    @network.parseNetwork( "192.168.1.0 / 24" )
  end

  alias :index :manage

  def save
    ## Review : Internationalization
    if ( params[:commit] != "Save".t )
      redirect_to( :action => "manage" )
      return false
    end

    settings = ArpEaterSettings.find( :first )
    settings = ArpEaterSettings.new if settings.nil?
    settings.update_attributes( params[:settings] )
    settings.save

    save_networks

    redirect_to( :action => "manage" )
  end

  private

  def save_networks
    network_list = []
    ids = params[:entries]
    networks = params[:networks]
    enabled = params[:enabled]
    spoof = params[:spoof]
    networks = params[:network]
    description = params[:description]
    gateways = params[:gateway]
    opportunistic = params[:opportunistic]

    position = 0
    unless ids.nil?
      ids.each do |key|
        network = ArpEaterNetworks.new( :enabled => enabled[key], :spoof => spoof[key],
                                        :opportunistic => opportunistic[key], 
                                        :gateway => gateways[key], :description => description[key] )

        network.parseNetwork( networks[key] )
        network_list << network
      end
    end

    ArpEaterNetworks.destroy_all()
    network_list.each { |network| network.save }

    os["arp_eater_manager"].commit
  end

end
