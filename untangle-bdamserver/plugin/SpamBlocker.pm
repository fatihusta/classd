package Mail::SpamAssassin::Plugin::SpamBlocker;

use Mail::SpamAssassin::Plugin;
use Mail::SpamAssassin::Logger;
use File::Temp qw(tempfile);
use IO::Socket::INET;
use strict;
use warnings;
use bytes;

use vars qw(@ISA);
@ISA = qw(Mail::SpamAssassin::Plugin);

sub new {
	my $class = shift;
	my $mailsaobject = shift;

	$class = ref($class) || $class;
	my $self = $class->SUPER::new($mailsaobject);
	bless ($self, $class);

	$self->set_config($mailsaobject->{conf});

	$self->register_eval_rule("spamblocker_check_message");

	return $self;
}

sub set_config {
	my($self, $conf) = @_;
	my @cmds = ();

	push (@cmds, {
		setting => 'spamblocker_host',
		type => $Mail::SpamAssassin::Conf::CONF_TYPE_STRING,
		default => 'localhost',
	});

	push (@cmds, {
		setting => 'spamblocker_port',
		type => $Mail::SpamAssassin::Conf::CONF_TYPE_STRING,
		default => '1344',
	});

	$conf->{parser}->register_commands(\@cmds);
}

sub spamblocker_check_message {
	my ($self, $pms) = @_;
	$self->_spamblocker_classify($pms) unless $pms->{'spamblocker_classified'};
	$pms->{spamblocker_flagged};
}

sub _spamblocker_classify {
	my ($self, $pms) = @_;

	# check the node license
	my $result = system("/usr/share/untangle/bin/ut-check-license.py untangle-node-spamblocker");
	$result >>= 8;
	if ($result != 0) {
		warn "SpamBlocker License Invalid: $result";
#		return;
	}

	# get the original message and size
	my $maildata = $pms->{msg}->get_pristine();
	my $mailsize = length($maildata);

	# dump the message in a temporary file
	(my $handle, my $filename) = tempfile("spamblocker-XXXXXX", SUFFIX => ".tmp", DIR => "/tmp", UNLINK => 0, CLEANUP => 0);
	print $handle $maildata;
	chmod(0666, $handle);
	close $handle;

	# open a connection to the daemon
	my $socket = new IO::Socket::INET(PeerHost => $pms->{conf}->{'spamblocker_host'}, PeerPort => $pms->{conf}->{'spamblocker_port'}, Proto => 'tcp');

	# transmit the scan request to the daemon
	# syntax = SCANFILE options filename - available options bits: (see docs for details)
	# 1 = BDAM_SCANOPT_ARCHIVES
	# 2 = BDAM_SCANOPT_PACKED
	# 4 = BDAM_SCANOPT_EMAILS
	# 8 = enable virus heuristics scanner
	# 16 = BDAM_SCANOPT_DISINFECT
	# 32 = return in-progress information
	# 64 = BDAM_SCANOPT_SPAMCHECK
	my $xmit = "SCANFILE 79 " . $filename . "\r\n";
	warn "XMIT: $xmit";

	# read the scan result
	my $recv = "";
	$socket->send($xmit);
	$socket->recv($recv, 1024);
	warn "RECV: $recv";

	# close the socket and remove the temporary file
	$socket->close();
	unlink($filename);

	# set the classified flag and add the status tag
	$pms->{'spamblocker_classified'} = 1;
	$pms->set_tag('SPAMBLOCKSTAT', $recv);

	# split the return string on spaces so we can isolate the status code
	# retlist[0] = status code
	# retlist[1] = threat type (V, S, W, D, A, M)
	# retlist[2] = other details
	my @retlist = split(' ', $recv, 3);

	# Return status of 227 indicates the file was clean.  Several
	# other codes could be returned if there is some problem scanning
	# the file, so we ignore those.  We only set the flag on 222 or 223
	# when virus or spam detection is effectively certain.

	# flag if infection detected
	if ($retlist[0] == 222) {
		$pms->{'spamblocker_flagged'} = 1;
	}

	# flag if infection suspected
	if ($retlist[0] == 223) {
		$pms->{'spamblocker_flagged'} = 1;
	}

	return;
}

1;

