class OSLibrary::ArpEaterManager < Alpaca::OS::ManagerBase
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

  ## Can't use nil, because the tested gateway may be nil.
  def is_auto( gateway )
    ArpEaterNetworks.is_gateway_auto?( gateway )
  end
end
