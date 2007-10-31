# Sebastien Delafond <seb@untangle.com>

require 'log4r'
require 'net/smtp'

# global functions
def runCommand(command, log)
  log.debug("Running '#{command}'")
  output = `#{command} 2>&1`
  rc = $?
  log.debug("RC: #{$?}")
  log.debug("Output: #{output}")
  return rc, output
end

class DistributionFactory
  def DistributionFactory.parse(distributionsFile, updatesFile)
    distributions = {}
    updaters      = {}

    parseFile(updatesFile).each { |paragraph|
      hash = parseParagraph(paragraph)
      updaters[hash["Name"]] = Updater.new(updatesFile, hash)
    }

    parseFile(distributionsFile).each { |paragraph|
      hash = parseParagraph(paragraph)
      distributions[hash["Codename"]] = Distribution.new(distributionsFile, hash, updaters[hash["Update"]])
    }

    return distributions
  end

  def DistributionFactory.parseFile(file)
    return File.open(file).read.split(/\n\n+/)
  end

  def DistributionFactory.parseParagraph(paragraph)
    hash = {}
    paragraph.split(/\n/).each { |line|
      line =~ /^(.+):\s+(.+)$/
      hash[$1] = $2
    }
    return hash
  end
end

class RepreproConfig
  attr_accessor :file, :basePath

  @@otherVariables = [ "file" ]
  @@arrayVariables = nil # to be redefined by subclasses

  @@logger = ( Log4r::Logger["RepreproConfig"] or Log4r::Logger.root() )
  def self.logger=(logger)
    @@logger = logger
  end

  def initialize(file, hash)
    @file = file
    @basePath = File.expand_path(File.join(File.dirname(@file), "..")) # `dirname conf/foo`/..
    hash.each_pair { |k,v|
      instanceVariable = uncapitalize(k)
      v = v.split(/\s+/) if @@arrayVariables.include?(instanceVariable)
      instance_variable_set("@#{instanceVariable}", v)
    }
  end

  def uncapitalize(string)
    return string[0..0].downcase + string[1..-1]
  end

  def capitalize(string)
    return string[0..0].upcase + string[1..-1]
  end

  def to_s
    s = ""
    instance_variables.each { |k|
      key = k[1..-1]
      value = instance_variable_get(k)
      # do not include "file" and friends
      next if @@otherVariables.include?(key) or value == nil
      # join arrays
      value = value.join(" ") if @@arrayVariables.include?(key)
      s << "#{capitalize(key)}: #{value}\n"
    }
    return s
  end

end

class Updater < RepreproConfig
  attr_accessor :name, :method, :suite, :components

  @@arrayVariables = [ "components" ]

  def initialize(file, hash)
    super(file, hash)
  end

end

class Distribution < RepreproConfig
  attr_accessor :origin, :suite, :version, :label, :codename, :debOverride, \
                :architectures, :components, :description, :log, :update, \
                :updater
  
  @@arrayVariables = [ "architectures", "components" ]
  @@otherVariables << "updater"

  def initialize(file, hash, updater = nil)
    @update = nil # this one is not always defined in the conf file
    @updater = updater
    super(file, hash)
    @baseCommand = "reprepro -V -b #{@basePath}"
  end

  def locked?
    return ( @suite =~ /stable/ and @suite !~ /unstable/ )
  end

  def hasPackage?(packageName)
    listCommand = "#{@baseCommand} list #{@codename} #{packageName}"
    rc, output = runCommand(listCommand, @@logger)
    return ( rc == 0 and output != "" )
  end

  def getPackageVersion(packageName)
    listCommand = "#{@baseCommand} list #{@codename} #{packageName}"
    rc, output = runCommand(listCommand, @@logger)
    return output.split(/\s+/)[-1]
  end

  def remove(packageName)
    removeCommand = "#{@baseCommand} remove #{@codename} #{packageName}"
    @@logger.info("Removing #{packageName} () from distribution #{@codename}")
    rc, output = runCommand(removeCommand, @@logger)
    return RemovalFailure.new("Failed to remove #{packageName}, rc=#{rc}, error was:\n\t#{output}") if rc != 0
  end

  def copyTo(distribution, packageName)
    copyCommand = "#{@baseCommand} copy #{distribution.codename} #{@codename} #{packageName}"
    rc, output = runCommand(copyCommand, @@logger)
    return CopyFailure.new("Failed to remove #{packageName}, rc=#{rc}, error was:\n\t#{output}") if rc != 0
  end

  def add(debianUpload)
    if debianUpload.is_a?(ChangeFileUpload) then # FIXME: refactor commands a bit
      command = "#{@baseCommand} include #{@codename} #{debianUpload.file}"
    else
      command = "#{@baseCommand} --priority #{debianUpload.priority} --component #{debianUpload.component} includedeb #{@codename} #{debianUpload.file}"
    end

    rc, output = runCommand(command, @@logger)

    if rc != 0 then
      if output =~ /No section was given for '#{debianUpload.name}', skipping/ then
        raise UploadFailureNoSection.new(output)
      elsif output =~ /No priority was given for '#{debianUpload.name}', skipping/ then
        raise UploadFailureNoPriority.new(output)
      elsif output =~ /is already registered with other md5sum/ then
        raise UploadFailureAlreadyUploaded.new(output)
      elsif output =~ /has md5sum.*while.*was expected/ then
        raise UploadFailureCorruptedUpload.new(output)        
      elsif output =~ /Cannot find file.*changes'/ then
        raise UploadFailureFileMissing.new(output)
      else
        raise UploadFailure.new("Something went wrong when adding #{debianUpload.name}\n\n" + output)
      end
    end
  end

end

# Custom exceptions
class UploadFailure < Exception ; end
class UploadFailureNoDistribution < UploadFailure ; end
class UploadFailureByPolicy < UploadFailure ; end
class UploadFailureNoSection < UploadFailure ; end
class UploadFailureNoPriority < UploadFailure ; end
class UploadFailureAlreadyUploaded < UploadFailure ; end
class UploadFailureFileMissing < UploadFailure ; end
class UploadFailureNotLocallyModifiedBuild < UploadFailure ; end
class UploadFailureCorruptedUpload < UploadFailure ; end
class RemovalFailure < Exception ; end
class CopyFailure < Exception ; end

class DebianUpload # Main base class

  attr_reader :file, :files, :name, :distribution, :uploader, :version, \
              :moveFiles, :dir, :maintainer, :uploader
  
  @@DEFAULT_DISTRIBUTION    = "chaos"
  @@DEFAULT_COMPONENT       = "upstream"
  @@DEFAULT_SECTION         = "utils"
  @@DEFAULT_PRIORITY        = "normal"

  @@logger = ( Log4r::Logger["DebianUpload"] or Log4r::Logger.root() )
  def self.logger=(logger)
    @@logger = logger
  end

  def initialize(file, moveFiles)
    @file = file
    @dir = File.dirname(@file)
    @name = File.basename(@file).gsub(/_.*/, "")
    @moveFiles = moveFiles
    @files = [ @file ]
  end

  def to_s
    s = "#{@name}\n"
    s += "  repository   = #{@repository}\n"
    s += "  distribution = #{@distribution}\n"
    s += "  version      = #{@version}\n"
    s += "  component    = #{@component}\n"
    s += "  maintainer   = #{@maintainer}\n"
    s += "  uploader     = #{@uploader}\n"
    s += "  files        =\n"
    @files.each { |file|
      s += "                 #{file}\n"
    }
    return s.strip()
  end

  def listFiles
    # list all files involved in the upload, one basename per line
    return @files.inject("") { |result, e|
      result += e.gsub(/#{@dir}\//, "") + "\n"
    }
  end

end

class PackageUpload < DebianUpload
  attr_accessor :priority, :component

  def initialize(file, moveFiles)
    super(file, moveFiles)
    @version                = @file.gsub(/.+?_(.+)_.+.deb$/, '\1')
    @distribution           = @@DEFAULT_DISTRIBUTION
    @component              = @@DEFAULT_COMPONENT
    @priority               = @@DEFAULT_PRIORITY
    @@logger.debug("Initialized #{self.class}: #{self.to_s}")
  end
end

class ChangeFileUpload < DebianUpload
  attr_reader :repository

  def initialize(file, moveFiles)
    super(file, moveFiles)
    if @file =~ /.*_.*-\d+(\w+)_.*/ then # valid 
      @repository = $1
    else # we'll fail this one at processing time
      @repository = nil
    end
    filesSection = false
    File.open(file).each { |line|
      line.strip!
      # FIXME: use a hash of /regex/ => :attribute
      case line
      when /^Source: / then
        @name = line.sub(/^Source: /, "")
      when /^Distribution: / then
        @distribution = line.sub(/^Distribution: /, "")
      when /^Maintainer: / then
        @maintainer = line.sub(/^Maintainer: /, "")
      when /^Changed-By: / then
        @uploader = line.sub(/^Changed-By: /, "")
      when /^Version: / then
        @version = line.sub(/^Version: /, "")
      when/^Files:/ then
        filesSection = true
        next
      when /^-----BEGIN PGP SIGNATURE-----/
        break # stop processing
      end

      if filesSection
        parts = line.split
        @files << File.join(@dir, parts[-1])
        @component = parts[2].split(/\//)[0] if not @component
      end
    }
    @@logger.debug("Initialized #{self.class}: #{self.to_s}")
  end

end

class Repository
  attr_reader :distributions

  @@DEFAULT_MAIL_RECIPIENTS = [ "rbscott@untangle.com", "seb@untangle.com" ]
  @@QA_MAIL_RECIPIENTS      = [ "ronni@untangle.com", "ksteele@untangle.com", "fariba@untangle.com" ]
  @@MAX_TRIES               = 3

  @@logger                   = ( Log4r::Logger["Repository"] or Log4r::Logger.root() )
  def self.logger=(logger)
    @@logger = logger
  end

  def initialize(basePath)
    @basePath                 = basePath
    @name                     = File.basename(@basePath)
    @processedPath            = File.join(@basePath, "processed")
    @failedPath               = File.join(@processedPath, "failed")
    @distributionFile         = File.join(@basePath, "conf/distributions")
    @updatesFiles             = File.join(@basePath, "conf/updates")
    @distributions            = DistributionFactory.parse(@distributionFile,
                                                          @updatesFiles)
    @lockedDistributions = @distributions.reject { |name, d| ! d.locked? }
    @unlockedDistributions = @distributions.reject { |name, d| d.locked? }
    # FIXME: find some other way...
    @testingDistributions = @distributions.reject { |name, d| d.suite !~ /testing/ }
    # FIXME: find those how ?
    @userDistributions = [ ]
    @@logger.debug("Initialized #{self.class}: #{self.to_s}")
  end

  def to_s
    s = "#{@name}\n"
    s += "  basePath      = #{@basePath}\n"
    s += "  distributions =\n"
    @distributions.each_key { |name|
      s += "                 #{name}\n"
    }
    return s
  end

  def scrubMailRecipients(list)
    # Remove non-untangle emails; strip names; add default recipients
    # map qa@untangle.com; uniq-ize
    list.delete_if { |r| not r =~ /@untangle\.com/ }
    list.concat(@@DEFAULT_MAIL_RECIPIENTS)
    list.map! { |r| r.gsub(/.*?<(.*)>/, '\1') }
    if list.grep(/buildbot/) != [] # no qa@untangle.com
      list.delete_if { |r| r =~ /buildbot/ }
      list.concat(@@QA_MAIL_RECIPIENTS)
    end
    return list.uniq
  end
  private :scrubMailRecipients

  def sendEmail(recipients, subject, body)
    recipients = scrubMailRecipients(recipients)
    myMessage = <<EOM
From: Incoming Queue Daemon <seb@untangle.com>
To: #{recipients.join(',')}
Subject: #{subject}

#{body}
EOM
    # FIXME: don't hardcode strings
    Net::SMTP.start('localhost', 25, 'localhost.localdomain') { |smtp|
      smtp.send_message(myMessage,"seb@untangle.com",*recipients)
    }
  end
  private :sendEmail

  def add(debianUpload, doEmail = false)
    @@logger.info("About to try adding: #{debianUpload.to_s}")
    success = false
    emailRecipients = [ debianUpload.uploader, debianUpload.maintainer ]
    tries = 0

    begin
      # first do a few policy checks

      # FIXME: those next 2 are lame, really; they shouldn't even reach here
      if not debianUpload.repository then
        raise UploadFailureNoDistribution.new("#{debianUpload.name} doesn't specify a repository to be added to.")
      end

      if debianUpload.repository != @name then
        raise UploadFailureNoDistribution.new("#{debianUpload.name} specifies an unknown repository (#{debianUpload.repository}) to be added to.")
      end

      if @testingDistributions.include?(debianUpload.distribution) and debianUpload.uploader !~ /(seb|rbscott|jdi)/i
        output = "#{debianUpload.name} was intended for #{debianUpload.distribution}, but you don't have permission to upload there."
        raise UploadFailureByPolicy.new(output)
      end

      # FIXME: dir-tay, needs some redesigning with regard to which policy checks apply to
      # which kind of uploads
      # FIXME: engineers names ? See FIXME in constructor
      if debianUpload.is_a?(ChangeFileUpload) and debianUpload.version !~ /svn/ and debianUpload.uploader !~ /(seb|rbscott|jdi)/i
        output = "#{debianUpload.version} doesn't contain 'svn', but you don't have permission to force the version."
        raise UploadFailureByPolicy.new(output)
      end

      if @lockedDistributions.include?(debianUpload.distribution)
        output = "#{debianUpload.name} was intended for #{debianUpload.distribution}, but this distribution is now locked."
        raise UploadFailureByPolicy.new(output)
      end

      if debianUpload.uploader =~ /root/i
        output = "#{debianUpload.name} was built by root, not processing"
        raise UploadFailureByPolicy.new(output)
      end

      # FIXME: QA distros
      if debianUpload.distribution =~ /(daily-dogfood|qa)/ and debianUpload.uploader !~ /(buildbot|seb|rbscott)/i
        output = "#{debianUpload.name} was intended for #{debianUpload.distribution}, but was not built by buildbot or a release master."
        raise UploadFailureByPolicy.new(output)
      end

      # FIXME
      if debianUpload.uploader =~ /buildbot/i and debianUpload.distribution !~ /(daily-dogfood|qa)/
        output = "#{debianUpload.name} was build by buildbot, but was intended for neither daily-dogfood nor qa."
        raise UploadFailureByPolicy.new(output)
      end

      if @userDistributions.include?(debianUpload.distribution) and not debianUpload.version =~ /\+[a-z]+[0-9]+T[0-9]+/i
        output = "#{debianUpload.name} was intended for user distribution '#{debianUpload.distribution}', but was not built from a locally modified SVN tree."
        raise UploadFailureNotLocallyModifiedBuild.new(output)
      end

      # then try to actually add the package
      @distributions[debianUpload.distribution].add(debianUpload)

      # if we arrive here, we have success (TM)
      success = true
    rescue UploadFailureAlreadyUploaded
      # copy it from the first distro that has it
      @unlockedDistributions.each_value { |d|
        if d == @distributions[debianUpload.distribution] then
          next # we're not going to copy from ourselves :)
        elsif d.hasPackage?(debianUpload.name) then # this package is present in this distro...
          version = d.getPackageVersion(debianUpload.name)
          if version == debianUpload.version then # ... with the same version -> copy it
            @@logger.info("Found #{debianUpload.name}, version: #{version}, in distribution #{d.codename}")
            d.copyTo(@distributions[debianUpload.distribution], debianUpload.name) 
            break # our job is done
          end
        end
      }
      # At this point, either we successfully copied from a distro that had this
      # specific version, or we were the one already having it. Which comes down
      # to the same result anyway.
      success = true
      body = "This package was already present in this repository, so it was simply copied over: #{debianUpload}"
    rescue UploadFailureFileMissing # sleep some, then retry
      sleep(3)
      tries += 1
      retry if tries < @@MAX_TRIES
      @@logger.warn("Due to missing file(s), gave up on adding: #{debianUpload}")
    # Those next 2 should be handled by the override file
    rescue UploadFailureNoSection # force the section, then retry
      # handled by overrides now
    rescue UploadFailureNoPriority # force the priority, then retry
      # handled by overrides now
    rescue Exception => e # give up, and warn on STDOUT + email
      # dumps error message on stdout, and possibly by email too
      subject = "Upload of #{debianUpload.name} failed (#{e.class})"
      body = e.message
      body += "\n" + e.backtrace.join("\n") if not e.is_a?(UploadFailure)
      @@logger.error("#{subject}\n#{body}")
      sendEmail(emailRecipients, subject, body) if doEmail
    ensure
      if success then
        # if we managed to get here, everything went fine
        # FIXME: email + log -> factorize
        subject = "Upload of #{debianUpload.name} succeeded"
        body = debianUpload.to_s if not body
        @@logger.info("#{subject}\n#{body}")
        sendEmail(emailRecipients, suject, body) if doEmail
        destination = @processedPath
      else
        destination = @failedPath        
      end
      # no matter what, try to remove files at this point
      tries = 0
      if debianUpload.moveFiles
        debianUpload.files.each { |file|
          begin
            File.rename(file, "#{destination}/#{File.basename(file)}")
          rescue # it's missing, do nothing
          end
        }
      end
    end
  end
end
