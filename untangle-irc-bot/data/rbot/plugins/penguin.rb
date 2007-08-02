class SyntaxException < Exception
end

class PenguinPlugin < Plugin

  def initialize
    super()
    @pipe = IO.popen("/opt/tuxdroid/bin/tuxsh")
  end

  def runPenguinCommand(m, command)
    begin
      @pipe.puts(command)
    rescue Exception => e
      handleException(m, e, true)
    end
  end

  def handleException(m, e, printStackTrace = true)
    m.reply "An exception happened: #{e.class} -> #{e.message}"
    if printStackTrace then
      e.backtrace.each { |line|
        m.reply "  #{line}"
      }
      m.reply "End of exception backtrace"
    end
  end

  def help(plugin, topic="")
    <<-eos
      pengin *command => Run command
    eos
  end

end

plugin = ExecPlugin.new
plugin.map 'penguin *args', :action => 'runPenguinCommand'

