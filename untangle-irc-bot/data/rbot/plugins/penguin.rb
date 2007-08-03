class SyntaxException < Exception
end

class PenguinPlugin < Plugin

  def initialize
    super()
    @pipe = IO.popen("python /opt/tuxdroid/api/python/tux2.py", "r+")
  end

  def consumeInput
    o = "0"
    while true do
      a = @pipe.gets
      break if a =~ /^0/
      o = a
    end
    return o
  end

  def runPenguinCommand(m, args)
    command = args[:command].join(" ")
    begin
      @pipe.puts(command)
      o = consumeInput
      if o != "0"
          m.reply(o)
      end
    rescue Exception => e
      handleException(m, e, false)
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

plugin = PenguinPlugin.new
plugin.map 'penguin *command', :action => 'runPenguinCommand'

