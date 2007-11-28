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

# classes
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

  def initialize(file, hash, updater = nil, useSudo = nil)
    @update = nil # this one is not always defined in the conf file
    @updater = updater
    super(file, hash)
    @baseCommand = useSudo ? "sudo " : ""
    @baseCommand << "reprepro -V -b #{@basePath}"
  end

  def <=>(other)
    return (@version or "" ) <=> ( other.version or "" )
  end

  def locked?
    return ( @suite =~ /stable/ and @suite !~ /unstable/ )
  end

  def developer?
    return @suite == nil
  end

  def hasPackage?(packageName)
    listCommand = "#{@baseCommand} list #{@codename} #{packageName}"
    rc, output = runCommand(listCommand, @@logger)
    return ( rc == 0 and output != "" )
  end

  def getPackageVersion(packageName)
    listCommand = "#{@baseCommand} list #{@codename} #{packageName}"
    rc, output = runCommand(listCommand, @@logger)
    return nil if output == ""
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
      command = "#{@baseCommand} --priority #{debianUpload.priority} --component #{debianUpload.component} includedeb #{@codename} #{debianUpload.files[1]}"
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
      elsif output =~ /(Cannot find file.*changes'|No such file or directory)/ then
        raise UploadFailureFileMissing.new(output)
      elsif output =~ /gpgme/ then
        raise UploadFailureGPGFailed.new(output)
      else
        raise UploadFailure.new("Something went wrong when adding #{debianUpload.name}\n\n" + output)
      end
    end
  end
end

class DebianPackage
  attr_reader :file, :name, :distribution, :version, \
              :architecture, :component, :repository

  @@logger = ( Log4r::Logger["DebianUpload"] or Log4r::Logger.root() )
  def self.logger=(logger)
    @@logger = logger
  end

  def initialize(file, repository, distribution, component, architecture)
    @file           = file
    @repository     = repository
    @distribution   = distribution
    @component      = component
    @architecture   = architecture
    @file           =~ /pool.*\/(.+?)_(.+?)_/
    @name, @version = $1, $2
  end

  def to_s
    return "#{@name}: #{@distribution} #{@component} #{@arch} #{@version}"
  end

end

# Custom exceptions
class UploadFailure < Exception ; end
class UploadFailureNoRepository < UploadFailure ; end
class UploadFailureUnknownDistribution < UploadFailure ; end
class UploadFailureByPolicy < UploadFailure ; end
class UploadFailureNoSection < UploadFailure ; end
class UploadFailureNoPriority < UploadFailure ; end
class UploadFailureAlreadyUploaded < UploadFailure ; end
class UploadFailureFileMissing < UploadFailure ; end
class UploadFailureNotLocallyModifiedBuild < UploadFailure ; end
class UploadFailureCorruptedUpload < UploadFailure ; end
class UploadFailureGPGFailed < UploadFailure ; end
class RemovalFailure < Exception ; end
class CopyFailure < Exception ; end

class DebianUpload # Main base class
  attr_reader :file, :files, :name, :distribution, :uploader, :version, \
              :moveFiles, :dir, :maintainer, :uploader, :repository, \
              :uploaderUsername
  
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

    re = /\.([a-z][^_]+?)_([a-z][^_]+?)\.manifest$/
    if @file =~ re then # valid 
      @repository = $1
      @distribution = $2
    else # we'll fail this one at processing time
      @repository = @distribution = nil
    end
    @files << @file.sub(re, ".deb")

    @version                = @files[1].gsub(/.+_(.+?)_.+?.deb$/, '\1')
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
    
    @uploaderUsername = @uploader.sub(/.*<(.+?)@.*>/, '\1') if @uploader

    @@logger.debug("Initialized #{self.class}: #{self.to_s}")
  end
end

class Repository
  attr_reader :distributions, :name, :developerDistributions, :testingDistributions, \
              :lockedDistributions, :unlockedDistributions

  # FIXME: domain/admins/qas by instance
  @@DOMAIN                  = "untangle.com"
  @@ADMINS                  = [ "jdi", "rbscott", "seb" ]
  @@QAS                     = [ "ksteele", "ronni" ]
  @@QA_UPLOADERS            = [ "buildbot", "qabuildbot" ]
  @@DEFAULT_MAIL_RECIPIENTS = @@ADMINS.map { |a| "#{a}@#{@@DOMAIN}" }
  @@QA_MAIL_RECIPIENTS      = @@QAS.map { |q| "#{q}@#{@@DOMAIN}" }
  @@TESTING_DISTRIBUTIONS   = [ "testing", "alpha" ]
  @@QA_DISTRIBUTIONS        = [ "daily-dogfood", "qa" ]
  @@MAX_TRIES               = 3
  
  @@logger                  = ( Log4r::Logger["Repository"] or Log4r::Logger.root() )
  def self.logger=(logger)
    @@logger = logger
  end

  # FIXME: domain/admins/qas by instance
  def initialize(basePath, useSudo = nil)
    @basePath                 = basePath
    @name                     = File.basename(@basePath)
    # FIMXE: create those 2 directories if they don't exist
    @processedPath            = File.join(@basePath, "processed")
    @failedPath               = File.join(@processedPath, "failed")
    @distributionFile         = File.join(@basePath, "conf/distributions")
    @updatesFiles             = File.join(@basePath, "conf/updates")
    @distributions            = DistributionFactory.parse(@distributionFile,
                                                          @updatesFiles)
    @lockedDistributions, @unlockedDistributions = {}, {}
    @testingDistributions, @developerDistributions = {}, {}

    @distributions.each { |name, d|
      new = { name => d }
      # locked/unlocked
      (d.locked? ? @lockedDistributions : @unlockedDistributions).merge!(new)
      # testing
      @testingDistributions.merge!(new) if @@TESTING_DISTRIBUTIONS.include?(d.suite)
      # dev
      @developerDistributions.merge!(new) if d.developer?      
    }

    @baseCommand = useSudo ? "sudo " : ""
    @baseCommand << "reprepro -V -b #{@basePath}"
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
    list.delete_if { |r| not r =~ /@#{@@DOMAIN}/ }
    list.concat(@@DEFAULT_MAIL_RECIPIENTS)
    list.map! { |r| r.gsub(/.*?<(.*)>/, '\1') }
    # change the next grep to: "does list have elements in QA_UPLOADERS"
    if list.grep(/buildbot/) != [] # no qa@untangle.com
      list.delete_if { |r| r =~ /buildbot/ }
      list.concat(@@QA_MAIL_RECIPIENTS)
    end
    return list.uniq
  end
  private :scrubMailRecipients

  def sendEmail(recipients, subject, body)
    recipients = scrubMailRecipients(recipients)
    # FIXME: don't hardcode strings
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
    @@logger.debug("Sent email to #{recipients.join(',')}")
  end
  private :sendEmail

  def getAllPackages
    pkgs = {}
    listAllCommand = "#{@baseCommand} dumpreferences"
    rc, output = runCommand(listAllCommand, @@logger)
    output.split(/\n/).grep(/\.deb$/).each { |line|
      distribution, component, architecture, file = line.split(/[\s|]/)
      pkg = DebianPackage.new(file, @name, distribution, component, architecture)
      if pkgs[pkg.name] then
        pkgs[pkg.name] << pkg
      else
        pkgs[pkg.name] = [ pkg, ]
      end
    }
    return pkgs
  end

  def add(debianUpload, doEmail = false)
    @@logger.info("About to try adding: #{debianUpload.to_s}")
    success = false
    emailRecipients = [ debianUpload.uploader, debianUpload.maintainer ]
    tries = 0

    begin
      # first, run all policy checks

      # FIXME: those next 2 are lame, really; we shouldn't even reach
      # this point in either of those cases
      if not debianUpload.repository then
        raise UploadFailureNoRepository.new("#{debianUpload.name} doesn't specify a repository to be added to.")
      end
      if debianUpload.repository != @name then
        raise UploadFailureNoRepository.new("#{debianUpload.name} specifies an unknown repository (#{debianUpload.repository}) to be added to.")
      end

      if not @distributions[debianUpload.distribution] then
        raise UploadFailureUnknownDistribution.new("#{debianUpload.name} specifies an unknown distribution (#{debianUpload.distribution}) to be added to.")
      end

      if @testingDistributions.keys.include?(debianUpload.distribution) and not @@ADMINS.include?(debianUpload.uploaderUsername)
        output = "#{debianUpload.name} was intended for #{debianUpload.distribution}, but you don't have permission to upload there."
        raise UploadFailureByPolicy.new(output)
      end

      if debianUpload.is_a?(ChangeFileUpload) and debianUpload.version !~ /svn/ and not @@ADMINS.include?(debianUpload.uploaderUsername)
        output = "#{debianUpload.version} doesn't contain 'svn', but you don't have permission to force the version."
        raise UploadFailureByPolicy.new(output)
      end

      if @lockedDistributions.keys.include?(debianUpload.distribution)
        output = "#{debianUpload.name} was intended for #{debianUpload.distribution}, but this distribution is now locked."
        raise UploadFailureByPolicy.new(output)
      end

      if debianUpload.uploaderUsername =~ /root/i
        output = "#{debianUpload.name} was built by root, not processing."
        raise UploadFailureByPolicy.new(output)
      end

      # QA distros/uploaders
      if @@QA_DISTRIBUTIONS.include?(debianUpload.distribution) and not (@@QA_UPLOADERS + @@ADMINS).include(debianUpload.uploaderUsername)
        output = "#{debianUpload.name} was intended for #{debianUpload.distribution}, but was not built by buildbot or a release master."
        raise UploadFailureByPolicy.new(output)
      end
      if (@@QA_UPLOADERS + @@ADMINS).include?(debianUpload.uploaderUsername) and not @@QA_DISTRIBUTIONS.include?(debianUpload.distribution)
        output = "#{debianUpload.name} was build by buildbot, but was intended for neither daily-dogfood nor qa."
        raise UploadFailureByPolicy.new(output)
      end

      if @developerDistributions.keys.include?(debianUpload.distribution) and not debianUpload.version =~ /\+[a-z]+[0-9]+T[0-9]+/i
        output = "#{debianUpload.name} was intended for user distribution '#{debianUpload.distribution}', but was not built from a locally modified SVN tree."
        raise UploadFailureNotLocallyModifiedBuild.new(output)
      end

      # all checks passed, now try to actually add the package
      @distributions[debianUpload.distribution].add(debianUpload)

      # if we arrive here, we have success (TM)
      success = true
    rescue UploadFailureAlreadyUploaded
      # copy it from the first distro that has it
      distro = @distributions[debianUpload.distribution]
      @unlockedDistributions.each_value { |d|
        if d == @distributions[debianUpload.distribution] then
          next # we're not going to copy from ourselves :)
        elsif d.hasPackage?(debianUpload.name) then # this package is present in this distro...
          version = d.getPackageVersion(debianUpload.name)
          if version == debianUpload.version then # ... with the same version -> copy it
            distro = d
            @@logger.info("Found #{debianUpload.name}, version: #{version}, in distribution #{distro.codename}")
            distro.copyTo(@distributions[debianUpload.distribution], debianUpload.name) 
            break # our job is done
          end
        end
      }
      # At this point, either we successfully copied from a distro that had this
      # specific version, or we were the one already having it, which comes down
      # to the same result anyway.
      success = true
      body = "This package was already present in the '#{debianUpload.repository}' repository, in distribution #{distro.codename}, with version '#{debianUpload.version}', so it was simply copied over."
    rescue UploadFailureFileMissing => e # sleep some, then retry
      sleep(3)
      tries += 1
      retry if tries < @@MAX_TRIES
      # FIXME: duplication with the catch-all rescue clause below...
    # Those next 2 should be handled by the override file
    rescue UploadFailureNoSection # force the section, then retry
      # handled by overrides now
    rescue UploadFailureNoPriority # force the priority, then retry
      # handled by overrides now
    rescue Exception => e # give up, and warn on STDOUT + email
      # will be handled in the "ensure" clause
    ensure # logging + emailing (if need) the result, and moving the files
      subject = "Upload of #{debianUpload.name} to #{debianUpload.repository}/#{debianUpload.distribution}"
      if success then
        # if we managed to get here, everything went fine
        subject << " succeeded"
        body = "" if not body
        body += "\n\n" + debianUpload.to_s
        @@logger.info("#{subject}\n#{body}")
        destination = @processedPath
      else
        subject << " failed (#{e.class})"
        body = e.message
        body += "\n\n" + debianUpload.to_s
        body += "\n\n" + e.backtrace.join("\n") if not e.is_a?(UploadFailure)
        @@logger.error("#{subject}\n#{body}")
        destination = @failedPath        
      end
      sendEmail(emailRecipients, subject, body) if doEmail
      # no matter what, try to remove files at this point
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
