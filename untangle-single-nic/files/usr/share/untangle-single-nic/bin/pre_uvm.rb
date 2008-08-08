#!/usr/bin/env ruby

require "dbi"
require "json"
require "cgi"
require "ftools"
require "logger"

## DELETE ME BEFORE RELEASING
Root="/home/rbscott/devel/untangle/web-ui/work/src/dist"

SingleNicFlag="/usr/share/untangle-arp-eater/flag"

CreatePopId="/usr/share/untangle/bin/createpopid.rb"
PopId="/usr/share/untangle/popid"

RegistrationInfo="/usr/share/untangle/registration.info"
RegistrationDone="/usr/share/untangle/.regdone"

logger = Logger.new( STDOUT )

def usage()
  puts "USAGE #{ARGV[0]} <config-file>"
  exit 254
end

## Utility from the alpaca to run a command with a timeout
def run_command( command, timeout = 30 )
  p = nil
  begin
    status = 127
    t = Thread.new do 
      p = IO.popen( command )
      pid, status = Process.wait2( p.pid )
      status = status.exitstatus
    end
    
    ## Kill the thread
    t.join( timeout )
    t.kill if t.alive?
    
    return status
  ensure
    p.close unless p.nil?
  end
end

def set_password_hash( config, dbh ) 
  password_hash = config["password"]
  if ( password_hash.nil? || /^[0-9a-fA-F]{48}$/.match( password_hash ).nil? )
    logger.warn( "Invalid password hash: #{password_hash}" )
    return
  end

  v = ""
  ( password_hash.length / 2 ).times { |n| v << ( "%c" % password_hash[2*n .. ( 2*n ) + 1].to_i( 16 )) }  

  admin_settings_id = dbh.select_one(<<SQL).first
SELECT nextval('hibernate_sequence');
SQL

  dbh.do("DELETE FROM u_user")
  dbh.do("DELETE FROM u_admin_settings")

  dbh.do("INSERT INTO u_admin_settings (admin_settings_id) VALUES (#{admin_settings_id})")  
  dbh.do("INSERT INTO u_user ( id, login, password, email, name, read_only, notes, send_alerts, admin_setting_id ) VALUES ( nextval('hibernate_sequence'),'admin','#{v.dump.gsub( "\"", "" )}','[no email]', 'System Administrator',false, '[no description]',false,#{admin_settings_id} )")
end

def setup_registration( config, dbh )
  unless File.exists?( CreatePopId )
    logger.warn( "Unable to create pop id, missing the script #{CreatePopId}" )
    return
  end

  unless ( status = run_command( CreatePopId, 15 )) == 0
    logger.warn( "Non-zero return code from pop id script. #{status}" )
    return
  end
  
  unless File.exists?( PopId )
    logger.warn( "POPID file doesn't exists: #{PopId}" )
    return
  end
  
  registration = config["registration"]

  popid = ""

  ## Popid is just the first line.
  File.open( PopId, "r" ) { |f| f.each_line { |l| popid = l ; break }}

  popid.strip!

  ## This will get updated later automatically.
  if popid.empty?
    logger.warn( "POPID is empty" )
    return
  end
    
  if registration.nil?
    logger.warn( "WARNING : Missing registration information, assuming bogus values." )
    registration["email"] = "unset@example.com"
    registration["name"] = "unset"
    registration["numseats"] = 5
  end

  url_string = []
  
  registration["regKey"] = popid

  name = registration["name"]
  unless name.nil?
    name = name.split( " " )
    
    case name.length
    when 0
      ## Nothing to do
    when 1
      registration["firstName"] = name[0]
    else
      registration["lastName"] = name.pop
      registration["firstName"] = name.join( " " )
    end
  end

  [ "regKey", "email", "name", "firstName", "lastName", "numseats", "find_untangle", "country", "purpose" ].each do |key|
    param = registration[key]

    next if param.nil?
    param = param.to_s
    param.strip!
    next if param.empty?

    url_string << "#{CGI.escape( key )}=#{CGI.escape( param.to_s )}"
  end
  
  File.rm_f( RegistrationInfo )
  File.rm_f( RegistrationDone )
  File.open( RegistrationInfo, "w" ) { |f| f.puts url_string.join( "&" ) }
end

def setup_alpaca( config_file )
  ## Initialize the settings for the alpaca.
Kernel.system(<<EOF)
BASE_DIR="/var/lib/rails/untangle-net-alpaca"
MONGREL_ENV="production"
[ -f /etc/default/untangle-net-alpaca ] && . /etc/default/untangle-net-alpaca
cd "${BASE_DIR}" || {
  echo "[`date`] Unable to change into ${BASE_DIR}"
  exit -1
}

rake --trace -s alpaca:preconfigure RAILS_ENV=${MONGREL_ENV} CONFIG_FILE="#{config_file}" || { 
  echo "[`date`] could not load preconfiguration settings."
  exit -2
}
EOF
end

usage if ARGV.length != 1

config_file = ARGV[0]

config = ""
File.open( config_file, "r" ) { |f| f.each_line { |l| config << l }}
config = ::JSON.parse( config )

## Disable the setup wizard by removing the router settings and clearing the database
Kernel.system( <<EOF )
echo "[`date`] Recreating the database for Single NIC Mode."
[ -f /etc/default/untangle-vm ] && . /etc/default/untangle-vm
dropdb -U postgres uvm
createuser -U postgres ${PG_CREATEUSER_OPTIONS} untangle 2>/dev/null
createdb -O postgres -U postgres uvm
#{Root}/usr/share/untangle/bin/update-schema settings uvm
#{Root}/usr/share/untangle/bin/update-schema events uvm
#{Root}/usr/share/untangle/bin/update-schema settings untangle-node-router
#{Root}/usr/share/untangle/bin/update-schema events untangle-node-router
EOF

## Insert the password for the user
dbh = DBI.connect('DBI:Pg:uvm', 'postgres')

set_password_hash( config, dbh )

setup_registration( config, dbh )

setup_alpaca( config_file )

