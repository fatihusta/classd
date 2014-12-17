package Mail::SpamAssassin::Plugin::MailShell;

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

	$self->register_eval_rule("mailshell_check_message_95_100");
	$self->register_eval_rule("mailshell_check_message_90_94");
	$self->register_eval_rule("mailshell_check_message_87_89");
	$self->register_eval_rule("mailshell_check_message_80_86");
	$self->register_eval_rule("mailshell_check_message_70_79");
	$self->register_eval_rule("mailshell_check_message_10_69");
	$self->register_eval_rule("mailshell_check_message_5_9");
	$self->register_eval_rule("mailshell_check_message_0_4");

	return $self;
}

sub set_config {
	my($self, $conf) = @_;
	my @cmds = ();

	# push (@cmds, {
	# 	setting => 'mailshell_spamc_config',
	# 	type => $Mail::SpamAssassin::Conf::CONF_TYPE_STRING,
	# 	default => 'localhost',
	# });

	$conf->{parser}->register_commands(\@cmds);
}

sub mailshell_check_message_95_100 {
	my ($self, $pms) = @_;
	$self->_mailshell_classify($pms) unless $pms->{'mailshell_classified'};
	$pms->{mailshell_score_95_100};
}

sub mailshell_check_message_90_94 {
	my ($self, $pms) = @_;
	$self->_mailshell_classify($pms) unless $pms->{'mailshell_classified'};
	$pms->{mailshell_score_90_94};
}

sub mailshell_check_message_87_89 {
	my ($self, $pms) = @_;
	$self->_mailshell_classify($pms) unless $pms->{'mailshell_classified'};
	$pms->{mailshell_score_87_89};
}

sub mailshell_check_message_80_86 {
	my ($self, $pms) = @_;
	$self->_mailshell_classify($pms) unless $pms->{'mailshell_classified'};
	$pms->{mailshell_score_80_86};
}

sub mailshell_check_message_70_79 {
	my ($self, $pms) = @_;
	$self->_mailshell_classify($pms) unless $pms->{'mailshell_classified'};
	$pms->{mailshell_score_70_79};
}

sub mailshell_check_message_10_69 {
	my ($self, $pms) = @_;
	$self->_mailshell_classify($pms) unless $pms->{'mailshell_classified'};
	$pms->{mailshell_score_10_69};
}

sub mailshell_check_message_5_9 {
	my ($self, $pms) = @_;
	$self->_mailshell_classify($pms) unless $pms->{'mailshell_classified'};
	$pms->{mailshell_score_5_9};
}

sub mailshell_check_message_0_4 {
	my ($self, $pms) = @_;
	$self->_mailshell_classify($pms) unless $pms->{'mailshell_classified'};
	$pms->{mailshell_score_0_4};
}


sub _mailshell_classify {
	my ($self, $pms) = @_;

	# set it to classified so if it fails after this point it won't re-evaluate
	$pms->{'mailshell_classified'} = 1;

	# check the node license
	my $result = system("/usr/share/untangle/bin/ut-check-license.py untangle-node-spamblocker");
	$result >>= 8;
	if ($result != 0) {
		warn "mailshell: Spam Blocker License Invalid: $result";
		return;
	}

	# get the original message and size
	my $maildata = $pms->{msg}->get_pristine();
	my $mailsize = length($maildata);

	# dump the message in a temporary file
	(my $handle, my $filename) = tempfile("mailshell-XXXXXX", SUFFIX => ".tmp", DIR => "/tmp", UNLINK => 0, CLEANUP => 0);
	print $handle $maildata;
	chmod(0666, $handle);
	close $handle;

	# run spamc
	my $output = `/usr/bin/spamc -R -p 784 < $filename`;
	if ( $? != 0 ) {
	    warn "mailshell: spamc failed: $?";
            unlink($filename);
	    return;
	}

        # remove the temporary file
        unlink($filename);

	# parse output
	my @lines = split(/\n/,$output);
	if ( scalar(@lines) < 2 ) { 
	    warn "mailshell: spamc invalid output: @lines";
	    return;
	}
	my @scores = split(/\//,$lines[0]);
	if ( scalar(@scores) < 2 ) { 
	    warn "mailshell: spamc score invalid output: $lines[0]";
	    return;
	}
	my $score = $scores[0];

	# set the appropriate flags
	$pms->{'mailshell_score_95_100'} = 0;
	$pms->{'mailshell_score_90_94'}  = 0;
	$pms->{'mailshell_score_87_89'}	 = 0;
	$pms->{'mailshell_score_80_86'}	 = 0;
	$pms->{'mailshell_score_70_79'}	 = 0;
	$pms->{'mailshell_score_10_69'}	 = 0;
	$pms->{'mailshell_score_5_9'}	 = 0;
	$pms->{'mailshell_score_0_4'}	 = 0;
	if ( $score >= 95 ) {
	    $pms->{'mailshell_score_95_100'} = 1;
	} elsif ( $score >= 90 ) {
	    $pms->{'mailshell_score_90_94'} = 1;
	} elsif ( $score >= 87 ) {
	    $pms->{'mailshell_score_87_89'} = 1;
	} elsif ( $score >= 80 ) {
	    $pms->{'mailshell_score_80_86'} = 1;
	} elsif ( $score >= 70 ) {
	    $pms->{'mailshell_score_70_79'} = 1;
	} elsif ( $score >= 10 ) {
	    $pms->{'mailshell_score_10_69'} = 1;
	} elsif ( $score >= 5 ) {
	    $pms->{'mailshell_score_5_9'} = 1;
	} elsif ( $score >= 0 ) {
	    $pms->{'mailshell_score_0_4'} = 1;
	} else {
	    warn 'mailshell: unknown score: $score'
	}

	return;
}

1;

