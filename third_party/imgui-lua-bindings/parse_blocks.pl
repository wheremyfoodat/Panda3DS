##!/usr/bin/perl
#use strict;
#use warnings;
#use diagnostics;
#
#my ($blocksref, $blocknamesref) = parse_blocks();
#
#my @blocks = @$blocksref;
#my @blocknames = @$blocknamesref;
#
#my $name;
#my $block;
#foreach $name (@blocknames) {
#  print "BLOCKNAME" . $name;
#}
#
#foreach $block (@blocks) {
#  print "---------------BLOCK START-----------\n";
#  print $block;
#  print "---------------BLOCK END -------------\n"
#}



# WARNING THIS FUNCTION WILL MESS UP ALL OF PARSING IF IT SEES
# A DIFFERENT NUMBER OF BLOCKNAMES AS BLOCKS...
# TODO FIX THIS ^

#returns list of string blocks from stdin


sub does_line_match_end_block {
  my $line = shift;

  # Make sure you take into account random whitespace that could happen by using [ ]*[\t]*
  my $match = 0;
  $match |= $line =~ m/^};[ ]*[\t]*\/\//;  # Semicolon,    comment
  $match |= $line =~ m/^};[ ]*[\t]*$/;     # Semicolon,    no comment
  $match |= $line =~ m/^}[ ]*[\t]*\/\//;   # No semicolon, comment
  $match |= $line =~ m/^}[ ]*[\t]*$/;      # No semicolon, no comment
  return $match;
}

sub does_line_match_begin_block {
  my $line = shift;

  my $match = 0;
  $match |= $line =~ m/^{[ ]*[\t]*$/;
  return $match
}

sub parse_blocks {
  my @blocks;
  my @blocknames;
  my $lastline;
  my $curBlock;
  while (my $line = <STDIN>) {
	if (does_line_match_begin_block($line)) {
		push @blocknames, $lastline;
		$curBlock = "";
		next;
	}
	  
	if (does_line_match_end_block($line)) {
	  push @blocks, $curBlock;

	  # Enforce the invariant that we should have the same number of elements in blocks / block names
	  if (scalar @blocks != scalar @blocknames) {
	  	print STDERR "The parser did a bad and has mismatched block open / close.";
	  	print STDERR "This is probably a problem with the regular expression that matches block open and close.";
	  	die
	  }
	    
	  $curBlock = "";
	    
	  next;
	}
	
	$curBlock .= $line . "\n";
	$lastline = $line;
  }
  return (\@blocks, \@blocknames);
}

1;
