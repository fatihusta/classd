## This should run from the RUSH shell.

require "dbi"
require "json"
require "cgi"
require "ftools"
require "logger"

logger = Logger.new( STDOUT )

## All of the pacakges should already be inside of the cache.
INST_OPTS = " -o DPkg::Options::=--force-confnew --yes --force-yes --fix-broken --purge --no-download "

def install_packages( config )
  packages = config["packages"]

  if ( packages.nil? || !( packages.is_a? Array ) || packages.empty? )
    logger.info( "No packages to install." )
    return
  end

  Kernel.system( "apt-get install #{INST_OPTS} #{packages}" )
end

def start_nodes( config )
  nodes = config["nodes"]

  if ( nodes.nil? || !( nodes.is_a? Array ) || nodes.empty? )
    logger.info( "No nodes to instantiate." )
    return
  end

  
  

end
