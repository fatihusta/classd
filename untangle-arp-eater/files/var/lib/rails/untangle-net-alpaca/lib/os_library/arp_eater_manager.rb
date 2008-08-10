class OSLibrary::ArpEaterManager < Alpaca::OS::ManagerBase
  AutoStrings = [ "auto", "automatic", "*" ]

  class ActiveHost
    def initialize( enabled, address, passive, gateway )
      @enabled, @address, @passive, @gateway = enabled, address, passive, gateway
    end

    attr_reader :address, :enabled, :passive, :gateway
  end

  ## This should commit and update all of the packet filter settings.
  def commit
    raise "base class, override in an os specific class"
  end

  def get_active_hosts
    raise "base class, override in an os specific class"
  end

  def is_auto( gateway )
    return true if gateway.nil? 
    gateway.strip!
    
    return true if gateway.empty? || AutoStrings.include?( gateway )
    false
  end
end
