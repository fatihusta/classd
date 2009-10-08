class OSLibrary::Null::ArpEaterManager < OSLibrary::ArpEaterManager
  include Singleton

  ## This should commit and update all of the packet filter settings.
  def commit
    puts "ignoring commit for arp eater manager"
  end

  def get_active_hosts
    puts "ignoring get_active_hosts"
    return []
  end
end
