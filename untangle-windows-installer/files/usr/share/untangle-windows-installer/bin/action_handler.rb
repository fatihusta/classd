#!/usr/bin/env ruby

require "ftools"

action_file = ENV["ACTION_FILE"]

ActionFile = action_file.nil? ? "/mnt/hgfs/untangle/action" : action_file
MaxActionSize = 2048

poll_timeout = ENV["POLL_TIMEOUT"]
PollTimeout = poll_timeout.nil? ? 1.0 : poll_timeout

module Actions
  Shutdown = "shutdown"
end

def get_action
  return nil unless File.exists? ActionFile
  
  ## File must have been corrupted or something.
  return nil if File.size( ActionFile ) > 2048
  
  ## Make sure you can write the file.
  return nil unless File.writable?( ActionFile )

  action = File.readlines( ActionFile )

  ## Always make sure to empty the contents of file.
  File.open( ActionFile, "w" ) { |f| f.truncate 0  }

  return nil unless ( action.length == 1 )

  action = action[0].downcase.strip
  return nil if action.empty?
  return action
end

def handle_action( action )
  case action
  when Actions::Shutdown
    Kernel.system( "shutdown -h now" )
    $is_running = false
  else
    puts "unknown action: #{action}"
  end
end

$is_running = true

while $is_running
  sleep PollTimeout
  action = get_action
  handle_action( action ) unless action.nil?
end

