#!/usr/bin/perl
#
#	git-森林
#	text-based tree visualisation
#	Copyright © Jan Engelhardt <jengelh [at] medozas de>, 2008
#
#	This program is free software; you can redistribute it and/or modify
#	it under the terms of the GNU General Public License as published by
#	the Free Software Foundation; either version 2 or 3 of the license.
#
use Getopt::Long;
use Git;
use strict;
use utf8;
use Encode qw(encode);
my $Repo          = Git->repository($ENV{"GIT_DIR"} || ".");
my $Pretty_fmt    = "format:%s";
my $Reverse_order = 0;
my $Show_all      = 0;
my $Show_rebase   = 1;
my $Style         = 1;
my $Subvine_depth = 2;
my $With_sha      = 0;
my %Color         = (
	"default" => "\e[0m", # ]
	"at"      => "\e[1;30m", # ]
	"hhead"   => "\e[1;31m", # ]
	"head"    => "\e[1;32m", # ]
	"ref"     => "\e[1;34m", # ]
	"remote"  => "\e[1;35m", # ]
	"sha"     => "\e[0;31m", # ]
	"tag"     => "\e[1;33m", # ]
	"tree"    => "\e[0;33m", # ]
);

&main();

sub main
{
	&Getopt::Long::Configure(qw(bundling pass_through));
	&GetOptions(
		"all"       => \$Show_all,
		"no-color"  => sub { %Color = (); },
		"no-rebase" => sub { $Show_rebase = 0; },
		"a"         => sub { $Pretty_fmt = "format:\e[1;30m(\e[0;32m%an\e[1;30m)\e[0m %s"; }, # ]]]]
		"pretty=s"  => \$Pretty_fmt,
		"reverse"   => \$Reverse_order,
		"svdepth=i" => \$Subvine_depth,
		"style=i"   => \$Style,
		"sha"       => \$With_sha,
	);
	++$Subvine_depth;
	if (substr($Pretty_fmt, 0, 7) ne "format:") {
		die "If you use --pretty, it must be in the form of --pretty=format:";
	}
	$Pretty_fmt = substr($Pretty_fmt, 7);
	while ($Pretty_fmt =~ /\%./g) {
		if ($& eq "\%b" || $& eq "\%n" || ($&.$') =~ /^\%x0a/i) {
			die "Cannot use \%b, \%n or \%x0a in --pretty=format:";
		}
	}
	if ($Show_all) {
		#
		# Give --all back. And add HEAD to include commits
		# in the rev list that belong to a detached HEAD.
		#
		unshift(@ARGV, "--all", "HEAD");
	}
	if ($Reverse_order) {
		tie(*STDOUT, "ReverseOutput");
	}
	&process();
	if ($Reverse_order) {
		untie *STDOUT;
	}
}

sub get_line_block
{
	my($fh, $max) = @_;

	while (scalar(@h::ist) < $max) {
		my $x;

		$x = <$fh>;
		if (!defined($x)) {
			last;
		}
		push(@h::ist, $x);
	}

	my @ret = (shift @h::ist);
	foreach (2..$max) {
		push(@ret, $h::ist[$_-2]);
	}
	return @ret;
}

sub process
{
	my(@vine);
	my $refs = &get_refs();
	my($fh, $fhc) = $Repo->command_output_pipe("log", "--date-order",
	                "--pretty=format:<%H><%h><%P>$Pretty_fmt", @ARGV);

	while (my($line, @next_sha) = get_line_block($fh, $Subvine_depth)) {
		if (!defined($line)) {
			last;
		}
		chomp $line;
		my($sha, $mini_sha, $parents, $msg) =
			($line =~ /^<(.*?)><(.*?)><(.*?)>(.*)/s);
		my @next_sha = map { ($_) = /^<(.*?)>/ } @next_sha;
		my @parents = split(" ", $parents);

		&vine_branch(\@vine, $sha);
		my $ra = &vine_commit(\@vine, $sha, \@parents);

		if (exists($refs->{$sha})) {
			print &vis_post(&vis_commit($ra),
			      $Color{at}."m".$Color{default});
			&ref_print($refs->{$sha});
		} else {
			print &vis_post(&vis_commit($ra, " "));
		}
		if ($With_sha) {
			print $msg, $Color{at}, encode('UTF-8', "──("), $Color{sha}, $mini_sha,
			      $Color{at}, ")", $Color{default}, "\n";
		} else {
			print $msg, "\n";
		}

		&vine_merge(\@vine, $sha, \@next_sha, \@parents);
	}
	$Repo->command_close_pipe($fh, $fhc);
}

sub get_refs
{
	my($fh, $c) = $Repo->command_output_pipe("show-ref");
	my $ret = {};

	while (defined(my $ln = <$fh>)) {
		chomp $ln;
		if (length($ln) == 0) {
			next;
		}

		my($sha, $name) = ($ln =~ /^(\S+)\s+(.*)/s);
		if (!exists($ret->{$sha})) {
			$ret->{$sha} = [];
		}
		push(@{$ret->{$sha}}, $name);
		if ($name =~ m{^refs/tags/}) {
			my $sub_sha = $Repo->command("log", "-1",
			              "--pretty=format:%H", $name);
			chomp $sub_sha;
			if ($sha ne $sub_sha) {
				push(@{$ret->{$sub_sha}}, $name);
			}
		}
	}

	$Repo->command_close_pipe($fh, $c);

	my $rebase = -e $Repo->repo_path()."/.dotest-merge/git-rebase-todo" &&
	             $Show_rebase;
	if ($rebase) {
		if (open(my $act_fh, $Repo->repo_path().
		    "/.dotest-merge/git-rebase-todo")) {
			my($curr) = (<$act_fh> =~ /^\S+\s+(\S+)/);
			$curr = $Repo->command("rev-parse", $curr);
			chomp $curr;
			unshift(@{$ret->{$curr}}, "rebase/cn");
			close $act_fh;
		}

		chomp(my $up   = $Repo->command("rev-parse", ".dotest-merge/upstream"));
		chomp(my $onto = $Repo->command("rev-parse", ".dotest-merge/onto"));
		chomp(my $old  = $Repo->command("rev-parse", ".dotest-merge/head"));
		unshift(@{$ret->{$up}}, "rebase/upstream");
		unshift(@{$ret->{$onto}}, "rebase/onto");
		unshift(@{$ret->{$old}}, "rebase/saved-HEAD");
	}

	my $head = $Repo->command("rev-parse", "HEAD");
	chomp $head;
	if ($rebase) {
		unshift(@{$ret->{$head}}, "rebase/new");
	}
	unshift(@{$ret->{$head}}, "HEAD");

	return $ret;
}

#
# ref_print - print a ref with color
# @s:	ref name
#
sub ref_print
{
	foreach my $symbol (@{shift @_}) {
		print $Color{at}, "[";
		if ($symbol eq "HEAD" || $symbol =~ m{^rebase/}) {
			print $Color{hhead}, $symbol;
		} elsif ($symbol =~ m{^refs/(remotes/[^/]+)/(.*)}s) {
			print $Color{remote}, $1, $Color{head}, "/$2";
		} elsif ($symbol =~ m{^refs/heads/(.*)}s) {
			print $Color{head}, $1;
		} elsif ($symbol =~ m{^refs/tags/(.*)}s) {
			print $Color{tag}, $1;
		} elsif ($symbol =~ m{^refs/(.*)}s) {
			print $Color{ref}, $1;
		}
		print $Color{at}, encode('UTF-8', "]──"), $Color{default};
	}
}

#
# vine_branch -
# @vine:	column array containing the expected parent IDs
# @rev:		commit ID
#
# Draws the branching vine matrix between a commit K and K^ (@rev).
#
sub vine_branch
{
	my($vine, $rev) = @_;
	my $idx;

	my($matched, $master) = (0, 0);
	my $ret;

	# Transform array into string
	for ($idx = 0; $idx <= $#$vine; ++$idx) {
		if (!defined($vine->[$idx])) {
			$ret .= " ";
			next;
		} elsif ($vine->[$idx] ne $rev) {
			$ret .= "I";
			next;
		}
		if (!$master && $idx % 2 == 0) {
			$ret .= "S";
			$master = 1;
		} else {
			$ret .= "s";
			$vine->[$idx] = undef;
		}
		++$matched;
	}

	if ($matched < 2) {
		return;
	}

	&remove_trailing_blanks($vine);
	print &vis_post(&vis_fan($ret, "branch")), "\n";
}

#
# vine_commit -
# @vine:	column array containing the expected IDs
# @rev:		commit ID
# @parents:	array of parent IDs
#
sub vine_commit
{
	my($vine, $rev, $parents) = @_;
	my $ret;

	for (my $i = 0; $i <= $#$vine; ++$i) {
		if (!defined($vine->[$i])) {
			$ret .= " ";
		} elsif ($vine->[$i] eq $rev) {
			$ret .= "C";
		} else {
			$ret .= "I";
		}
	}

	if ($ret !~ /C/) {
		# Not having produced a C before means this is a tip
		my $i;
		for ($i = &round_down2($#$vine); $i >= 0; $i -= 2) {
			if (substr($ret, $i, 1) eq " ") {
				substr($ret, $i, 1) = "t";
				$vine->[$i] = $rev;
				last;
			}
		}
		if ($i < 0) {
			if (scalar(@$vine) % 2 != 0) {
				push(@$vine, undef);
				$ret .= " ";
			}
			$ret .= "t";
			push(@$vine, $rev);
		}
	}

	&remove_trailing_blanks($vine);

	if (scalar(@$parents) == 0) {
		# tree root
		$ret =~ tr/C/r/;
	}

	return $ret;
}

#
# vine_merge -
# @vine:	column array containing the expected parent IDs
# @rev:		commit ID
# @next_rev:	next commit ID in the revision list
# @parents:	parent IDs of @rev
#
# Draws the merging vine matrix between a commit K (@rev) and K^ (@parents).
#
sub vine_merge
{
	my($vine, $rev, $next_rev, $parents) = @_;
	my $orig_vine = -1;
	my @slot;
	my($ret, $max);

	for (my $i = 0; $i <= $#$vine; ++$i) {
		if ($vine->[$i] eq $rev) {
			$orig_vine = $i;
			last;
		}
	}

	if ($orig_vine == -1) {
		die "vine_commit() did not add this vine.";
	}

	if (scalar(@$parents) <= 1) {
		#
		# A single parent does not need a visual. Update and return.
		#
		$vine->[$orig_vine] = $parents->[0];
		&remove_trailing_blanks($vine);
		return;
	}

	#
	# Put previously seen branches in the vine subcolumns
	# Need to keep at least one parent for the slot algorithm below.
	#
	for (my $j = 0; $j <= $#$parents && $#$parents > 0; ++$j) {
		for (my $idx = 0; $idx <= $#$vine; ++$idx) {
			if ($vine->[$idx] ne $parents->[$j] ||
			    !grep { my $z = $vine->[$idx]; /^\Q$z\E$/ }
			    @$next_rev) {
				next;
			}
			if ($idx == $orig_vine) {
				die "Should not really happen";
			}
			if ($idx < $orig_vine) {
				my $p = $idx + 1;
				if (defined($vine->[$p])) {
					$p = $idx - 1;
				}
				if (defined($vine->[$p])) {
					last;
				}
				$vine->[$p] = $parents->[$j];
				str_expand(\$ret, $p + 1);
				substr($ret, $p, 1) = "s";
			} else {
				my $p = $idx - 1;
				if (defined($vine->[$p]) || $p < 0) {
					$p = $idx + 1;
				}
				if (defined($vine->[$p])) {
					last;
				}
				$vine->[$p] = $parents->[$j];
				str_expand(\$ret, $p + 1);
				substr($ret, $p, 1) = "s";
			}
			splice(@$parents, $j, 1);
			--$j; # outer loop
			last; # inner loop
		}
	}

	#
	# Find some good spots to split out into and record columns
	# that will be used soon in the @slot list.
	#
	push(@slot, $orig_vine);
	my $parent = 0;

	for (my $seeker = 2; $parent < $#$parents &&
	    $seeker < 2 + $#$vine; ++$seeker)
	{
		my $idx = ($seeker % 2 == 0) ? -1 : 1;
		$idx   *= int($seeker / 2);
		$idx   *= 2;
		$idx   += $orig_vine;

		if ($idx >= 0 && $idx <= $#$vine && !defined($vine->[$idx])) {
			push(@slot, $idx);
			$vine->[$idx] = "0" x 40;
			++$parent;
		}
	}
	for (my $idx = $orig_vine + 2; $parent < $#$parents; $idx += 2) {
		if (!defined($vine->[$idx])) {
			push(@slot, $idx);
			++$parent;
		}
	}

	if (scalar(@slot) != scalar(@$parents)) {
		die "Serious internal problem";
	}

	@slot = sort { $a <=> $b } @slot;
	$max  = scalar(@$vine) + 2 * scalar(@slot);

	for (my $i = 0; $i < $max; ++$i) {
		str_expand(\$ret, $i + 1);
		if ($#slot >= 0 && $i == $slot[0]) {
			shift @slot;
			$vine->[$i] = shift @$parents;
			substr($ret, $i, 1) = ($i == $orig_vine) ? "S" : "s";
		} elsif (substr($ret, $i, 1) eq "s") {
			; # keep existing fanouts
		} elsif (defined($vine->[$i])) {
			substr($ret, $i, 1) = "I";
		} else {
			substr($ret, $i, 1) = " ";
		}
	}

	print &vis_post(&vis_fan($ret, "merge")), "\n";
}

#
# vis_* - transform control string into usable graphic
#
# To cut down on code, the three vine_* functions produce only a dumb,
# but already unambiguous, control string which needs some processing
# before it is ready for public display.
#

sub vis_commit
{
	my $s = shift @_;
	my $f = shift @_;
	$s =~ s/ +$//gs;
	if (defined $f) {
		$s .= $f;
	}
	return $s;
}

sub vis_fan
{
	my $s = shift @_;
	my $b = shift(@_) eq "branch";

	$s =~ s{s.*s}{
		$_ = $&;
		$_ =~ tr/ I/DO/;
		$_;
	}ei;

	# Transform an ODODO.. sequence into a contiguous overpass.
	$s =~ s{O[DO]+O}{"O" x length($&)}eg;

	# Do left/right edge transformation
	$s =~ s{(s.*)S(.*s)}{&vis_fan3($1, $2)}es ||
	$s =~ s{(s.*)S}{&vis_fan2L($1)."B"}es ||
	$s =~ s{S(.*s)}{"A".&vis_fan2R($1)}es ||
	die "Should not come here";

	if ($b) {
		$s =~ tr/efg/xyz/;
	}

	return $s;
}

sub vis_fan2L
{
	my $l = shift @_;
	$l =~ s/^s/e/;
	$l =~ s/s/f/g;
	return $l;
}

sub vis_fan2R
{
	my $r = shift @_;
	$r =~ s/s$/g/;
	$r =~ s/s/f/g;
	return $r;
}

sub vis_fan3
{
	my($l, $r) = @_;
	$l =~ s/^s/e/;
	$l =~ s/s/f/g;
	$r =~ s/s$/g/;
	$r =~ s/s/f/g;
	return "${l}K$r";
}

sub vis_xfrm
{
	# A: branch to right
	# B: branch to right
	# C: commit
	# D:
	# e: merge visual left (╔)
	# f: merge visual center (╦)
	# g: merge visual right (╗)
	# I: straight line (║)
	# K: branch visual split (╬)
	# m: single line (─)
	# O: overpass (≡)
	# r: root (╙)
	# t: tip (╓)
	# x: branch visual left (╚)
	# y: branch visual center (╩)
	# z: branch visual right (╝)
	# *: filler

	my $s = shift @_;
	my $spc = shift @_;
	if ($spc) {
		$s =~ s{[Ctr].*}{
			$_ = $&;
			$_ =~ s{ }{\*}g;
			$_;
		}esg;
	}

	if ($Reverse_order) {
		$s =~ tr/efg.rt.xyz/xyz.tr.efg/;
	}

	if ($Style == 1) {
		$s =~ tr/ABCD.efg.IKO.mrt.xyz/├┤├─.┌┬┐.│┼≡.─└┌.└┴┘/;
	} elsif ($Style == 2) {
		$s =~ tr/ABCD.efg.IKO.mrt.xyz/╠╣╟═.╔╦╗.║╬═.─╙╓.╚╩╝/;
	} elsif ($Style == 10) {
		$s =~ tr/ABCD.efg.IKO.mrt.xyz/├┤├─.╭┬╮.│┼≡.─└┌.╰┴╯/;
	} elsif ($Style == 15) {
		$s =~ tr/ABCD.efg.IKO.mrt.xyz/┣┫┣━.┏┳┓.┃╋☰.━┗┏.┗┻┛/;
	}
	return $s;
}

#
# vis_post - post-process/transform vine graphic
# Apply user-specific style transformation.
#
sub vis_post
{
	my $s = shift @_;
	my $f = shift @_;

	$s = vis_xfrm($s, defined $f);
	$f =~ s/^([^\x1b]+)/vis_xfrm($1)/e;
	$f =~ s/(\x1b.*?m)([^\x1b]+)/$1.vis_xfrm($2)/eg;
	if (defined $f) {
		$s =~ s/\*/$f/g;
		$s =~ s{\Q$Color{default}\E}{$&.$Color{tree}}egs;
		$s .= $f;
	}

	return $Color{tree}, encode('UTF-8', $s), $Color{default};
}

sub remove_trailing_blanks
{
	my $a = shift @_;

	while (scalar(@$a) > 0 && !defined($a->[$#$a])) {
		pop(@$a);
	}
}

sub round_down2
{
	my $i = shift @_;
	if ($i < 0) {
		return $i;
	}
	return $i & ~1;
}

sub str_expand
{
	my $r = shift @_;
	my $l = shift @_;

	if (length($$r) < $l) {
		$$r .= " " x ($l - length($$r));
	}
}

package ReverseOutput;
require Tie::Handle;
@ReverseOutput::ISA = qw(Tie::Handle);

sub TIEHANDLE
{
	my $class = shift @_;
	my $self  = {};

	open($self->{fh}, ">&STDOUT");

	return bless $self, $class;
}

sub PRINT
{
	my $self = shift @_;
	my $fh   = $self->{fh};

	$self->{saved} .= join($\, @_);
}

sub UNTIE
{
	my $self = shift @_;
	my $fh   = $self->{fh};

	print $fh join($/, reverse split(/\n/s, $self->{saved}));
	print $fh "\n";
	undef $self->{saved};
}
