#!/usr/bin/perl
# mkrom.pl code.bin bank segments [last.bin]
# Segment 0 = code + zeros, the rest filled with their number; last.bin
# (page3.asm) goes at the start of the last segment.
use strict;
use warnings;

my ($rom, $bank, $segs, $last) = @ARGV;
die "usage: $0 code.bin bank segments [last.bin]\n" unless defined $segs;

sub slurp {
	my ($f) = @_;
	open my $fh, '<:raw', $f or die "$f: $!\n";
	local $/;
	my $code = <$fh>;
	die "$f: code is " . length($code) . " bytes, over 0x800\n"
	    if length($code) > 0x800;
	return $code;
}

my @code = (slurp($rom));
$code[$segs - 1] = slurp($last) if defined $last;

open my $out, '>:raw', $rom or die "$rom: $!\n";
for my $s (0 .. $segs - 1) {
	my $c = $code[$s] // '';
	print $out $c, chr($s) x ($bank - length $c);
}
close $out or die "$rom: $!\n";
