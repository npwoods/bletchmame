#!/usr/bin/perl

###################################################################################
# process_version.pl - Generates BletchMAME version information in various forms  #
###################################################################################

if (!(<STDIN> =~ /v([0-9]+)\.([0-9]+)(\-[0-9]+)?/))
{
	die "Cannot process build string";
}
my $major = $1;
my $minor = $2;
my $subbuild = 0;
my $build = $3;
$build =~ s/^\-//;
if ($build eq "")
{
	$build = 0;
}

if ($ARGV[0] eq "--versionhdr")
{
	# header file
	print "#define BLETCHMAME_VERSION_COMMASEP	$major,$minor,$subbuild,$build\n";
	print "#define BLETCHMAME_VERSION_STRING	\"$major.$minor";
	if ($subbuild != 0 || $build != 0)
	{
		print ".$subbuild.$build";
	}
	print "\\0\"\n";
}
else
{
	# simple version text
	print "$major.$minor.$subbuild.$build\n";
}
