#! /usr/bin/ruby

# Sebastien Delafond <seb@untangle.com>

require 'optparse'
require 'net/smtp'

# constants
REP = "/var/www/untangle"
DISTS = "#{REP}/dists"
INCOMING = "#{REP}/incoming"
PROCESSED = "#{REP}/processed"
FAILED = "#{PROCESSED}/failed"

DELAY = 60 # in seconds
DISTRIBUTIONS = Dir.entries(DISTS).delete_if { |f| f =~ /\./ or File.symlink?(f) }
# FIXME: please extract those dynamically from the distributions file; this
# hardcoding is so horrendous even rbscott wouldn't settle for it
TESTING_DISTRIBUTIONS = [ "testing", "mustang-2" ]
LOCKED_DISTRIBUTIONS = [ "stable", "mustang-2", "mustang-1", "oldstable", "mustang" ]
UNLOCKED_DISTRIBUTIONS = DISTRIBUTIONS.delete_if { |d| LOCKED_DISTRIBUTIONS.include?(d) }
USER_DISTRIBUTIONS = [ "amread", "arthur", "dmorris", "jdi", "khilton", "rbscott", "seb" ]
DEFAULT_DISTRIBUTION = "chaos"
DEFAULT_COMPONENT = "upstream"
DEFAULT_SECTION = "utils"
DEFAULT_PRIORITY = "normal"
DEFAULT_MAIL_RECIPIENTS = [ "rbscott@untangle.com", "seb@untangle.com" ]
QA_MAIL_RECIPIENTS = [ "ronni@untangle.com", "ksteele@untangle.com", "fariba@untangle.com" ]
MAX_TRIES = 3

# global functions
def email(recipients, subject, body)
  recipients.delete_if { |r| not r =~ /@untangle\.com/ }
  recipients.map! { |r|
    r.gsub(/.*?<(.*)>/, '\1')
  }
  recipientsString = recipients.join(',')
  myMessage = <<EOM
From: Incoming Queue Daemon <seb@untangle.com>
To: #{recipientsString}
Subject: #{subject}

#{body}
EOM

  Net::SMTP.start('localhost', 25, 'localhost.localdomain') { |smtp|
    smtp.send_message(myMessage,"seb@untangle.com",*recipients)
  }
end

# Custom exceptions
class UploadFailure < Exception
end
class UploadFailureByPolicy < UploadFailure
end
class UploadFailureNoSection < UploadFailure
end
class UploadFailureNoPriority < UploadFailure
end
class UploadFailureAlreadyUploaded < UploadFailure
end
class UploadFailureFileMissing < UploadFailure
end
class UploadFailureNotLocallyModifiedBuild < UploadFailure
end

class DebianUpload # Main base class

  @@doEmailSuccess = true
  @@doEmailFailure = true

  attr_reader :files, :distribution, :uploader, :version

  def initialize(file, moveFiles = true)
    @file = file
    @moveFiles = moveFiles
    @files = [ @file ]
  end

  def to_s
    s = "#{@file}\n"
    s += "  distribution = #{@distribution}\n"
    s += "  component = #{@component}\n"
    s += "  maintainer = #{@maintainer}\n"
    s += "  uploader = #{@uploader}\n"
    s += "  version = #{@version}\n"
    s += "  files =\n"
    @files.each { |file|
      s += "    #{file}\n"
    }
    return s
  end

  def listFiles
    # list all files involved in the upload, one basename per line
    return @files.inject("") { |result, e|
      result += e.gsub(/#{INCOMING}\//, "") + "\n"
    }
  end

  def handleFailure(e)
    # dumps error message on stdout, and possibly by email too
    subject = "Upload of #{@name} failed (#{e.class})"
    body = e.message
    body += "\n" + e.backtrace.join("\n") if not e.is_a?(UploadFailure)
    puts "#{subject}\n#{body}"
    email(@emailRecipientsFailure,
          subject,
          body) if @@doEmailFailure
  end

  def addToRepository
    destination = FAILED
    tries = 0
    begin
      # first do a few policy checks
      if TESTING_DISTRIBUTIONS.include?(@distribution) and @uploader !~ /(seb|rbscott|jdi)/i
        output = "#{@name} was intended for #{@distribution}, but you don't have permission to upload there."
        raise UploadFailureByPolicy.new(output)
      end

      # FIXME: dir-tay, needs some redesigning with regard to which policy checks apply to
      # which kind of uploads
      if is_a?(ChangeFileUpload) and @version !~ /svn/ and @uploader !~ /(seb|rbscott|jdi)/i
        output = "#{@version} doesn't contain 'svn', but you don't have permission to force the version."
        raise UploadFailureByPolicy.new(output)
      end

      if LOCKED_DISTRIBUTIONS.include?(@distribution)
        output = "#{@name} was intended for #{@distribution}, but this distribution is now locked."
        raise UploadFailureByPolicy.new(output)
      end

      if @uploader =~ /root/i
        output = "#{@name} was built by root, not processing"
        raise UploadFailureByPolicy.new(output)
      end

      if @distribution =~ /(daily-dogfood|qa)/ and @uploader !~ /(buildbot|seb|rbscott)/i
        output = "#{@name} was intended for #{@distribution}, but was not built by buildbot or a release master."
        raise UploadFailureByPolicy.new(output)
      end

      if @uploader =~ /buildbot/i and @distribution !~ /(daily-dogfood|qa)/
        output = "#{@name} was build by buildbot, but was intended for neither daily-dogfood nor qa."
        raise UploadFailureByPolicy.new(output)
      end

      if USER_DISTRIBUTIONS.include?(@distribution) and not @version =~ /\+[a-z]+[0-9]+T[0-9]+/i
        output = "#{@name} was intended for user distribution '#{@distribution}', but was not built from a locally modified SVN tree."
        raise UploadFailureNotLocallyModifiedBuild.new(output)
      end

      # then try to actually add the package
      output = `#{@command} 2>&1`
      puts output
      if $? != 0
        if output =~ /No section was given for '#{@name}', skipping/ then
          raise UploadFailureNoSection.new(output)
        elsif output =~ /No priority was given for '#{@name}', skipping/ then
          raise UploadFailureNoPriority.new(output)
        elsif output =~ /is already registered with other md5sum/ then
          raise UploadFailureAlreadyUploaded.new(output)
        elsif output =~ /Cannot find file.*changes'/ then
          raise UploadFailureFileMissing.new(output)
        else
          raise UploadFailure.new("Something went wrong when adding #{@name}\n\n" + output)
        end
      end

      destination = PROCESSED

      email(@emailRecipientsSuccess,
            "Upload of #{@name} succeeded",
            to_s) if @@doEmailSuccess

    rescue UploadFailureAlreadyUploaded
      # first remove it from all distros that have it...
      UNLOCKED_DISTRIBUTIONS.each { |d|
        listCommand = "reprepro -V -b #{REP} list #{d} #{@name}"
        output = `#{listCommand} 2>&1`
#        puts d, output
        if $? == 0 and output != "" then # this package is present in this distro...
          version = output.split(/\s+/)[-1]
#          puts "version: #{version}, matched: #{@version}"
          if version == @version then # ... with the same version -> remove it
            @files.each { |f|
              if f =~ /.+\/(.+?)_.+\.dsc$/ then
                sourceName = $1
                removeCommand = "reprepro -V -b #{REP} remove #{d} #{sourceName}"
                output = `#{removeCommand} 2>&1`
              elsif f =~ /.+\/(.+?)_.+\.deb$/ then
                packageName = $1
                removeCommand = "reprepro -V -b #{REP} remove #{d} #{packageName}"
                output = `#{removeCommand} 2>&1`
              end
            }
          end
        end
      }
      retry # ... then retry
    rescue UploadFailureFileMissing # sleep some, then retry
      sleep(3)
      tries += 1
      retry if tries < MAX_TRIES
# This should now be handled by the override file
    rescue UploadFailureNoSection # force the section, then retry
      @command = @command.gsub!(/\-V/, "-V --section #{DEFAULT_SECTION}")
      retry
    rescue UploadFailureNoPriority # force the priority, then retry
      @command = @command.gsub!(/\-V/, "-V --priority #{DEFAULT_PRIORITY}")
      retry
    rescue Exception => e # give up, and warn on STDOUT + email
      handleFailure(e)
    ensure # no matter what, try to remove files at this point
      tries = 0
      if @moveFiles
        @files.each { |file|
          File.rename(file, "#{destination}/#{File.basename(file)}")
        }
      end
    end
  end
end

class PackageUpload < DebianUpload
  def initialize(file, moveFiles)
    super(file, moveFiles)
    @name = File.basename(@file).gsub(/_.*/, "")
    @distribution = DEFAULT_DISTRIBUTION
    @component = DEFAULT_COMPONENT
    @command = "reprepro -V -b #{REP} --component #{@component} includedeb #{@distribution} #{@file}"
    @emailRecipientsSuccess = DEFAULT_MAIL_RECIPIENTS
    @emailRecipientsFailure = DEFAULT_MAIL_RECIPIENTS
  end
end

class ChangeFileUpload < DebianUpload
  def initialize(file, moveFiles)
    super(file, moveFiles)
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
        @files << INCOMING + "/" + parts[-1]
        @component = parts[2].split(/\//)[0] if not @component
      end
    }
    @command = "reprepro -Vb #{REP} include #{@distribution} #{@file}"
    @emailRecipientsSuccess = [ @uploader, @maintainer ].uniq
    @emailRecipientsFailure = @emailRecipientsSuccess + DEFAULT_MAIL_RECIPIENTS
    # FIXME: make that a function
    if @emailRecipientsSuccess.grep(/buildbot/) != [] # no qa@untangle.com
      @emailRecipientsSuccess.delete_if { |e| e =~ /buildbot/ }
      @emailRecipientsSuccess += QA_MAIL_RECIPIENTS
    end
    if @emailRecipientsFailure.grep(/buildbot/) != [] # no qa@untangle.com
      @emailRecipientsFailure.delete_if { |e| e =~ /buildbot/ }
      @emailRecipientsFailure += QA_MAIL_RECIPIENTS
    end
    @emailRecipientsFailure.uniq!
  end
end

def processLocation(location, options)
  if File.directory?(location)
    Dir["#{location}/*.changes"].each { |file|
      cfu = ChangeFileUpload.new(file, options[:moveFiles])
      cfu.addToRepository
    }

    Dir["#{location}/*.deb"].each { |file|
      if not File.file?(location + "/" + file.sub(/_.?\.deb/, ".dsc")) then
        # we're good, this wasn't a source pkg upload taking too long to be uploaded...
        pu = PackageUpload.new(file, options[:moveFiles])
        pu.addToRepository
      end
    }
  else
    if location =~ /\.changes$/
      cfu = ChangeFileUpload.new(location, options[:moveFiles])
      cfu.addToRepository
    else
      pu = PackageUpload.new(location, options[:moveFiles])
      pu.addToRepository
    end
  end
end

def parseCommandLineArgs(args)
  options = {}
  options[:daemonize] = false
  options[:moveFiles] = false

  opts = OptionParser.new do |opts|
    opts.on("-d", "--daemonize", "Daemonize the process") do |s|
      options[:daemonize] = true
    end
    opts.on("-m", "--move-files", "Move files after they're processed") do |s|
      options[:moveFiles] = true
    end
    opts.on_tail("-h", "--help", "Show this usage statement") do |h|
      puts opts
    end
  end

  begin
    opts.parse!(args)
  rescue Exception => e
    puts e, "", opts
    exit
  end

  # location...
  if args.length == 1
    location = args[0]
  else
    location = INCOMING
  end

  return location, options
end

# Main
location, options = parseCommandLineArgs(ARGV)

if options[:daemonize] then
  pid = fork do
    loop do
      processLocation(location, options)
      sleep(DELAY)
    end
  end
  Process.detach(pid)
else
  processLocation(location, options)
end
