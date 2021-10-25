#!/usr/bin/env perl

#######################################################################################
# validate_dllimports.pl - Validates that the list of dependencies are what we expect #
#######################################################################################

# These are the DLLs we allow
my @allowed_dlls =
(
	"advapi32.dll",
	"dwmapi.dll",
	"gdi32.dll",
	"imm32.dll",
	"kernel32.dll",
	"msvcrt.dll",
	"netapi32.dll",
	"ole32.dll",
	"oleaut32.dll",
	"shell32.dll",
	"user32.dll",
	"userenv.dll",
	"uxtheme.dll",
	"version.dll",
	"winmm.dll",
	"ws2_32.dll",
	"wtsapi32.dll"
);
my %allowed_dlls_hash = map {$_ => 1} @allowed_dlls;
my $bad_import_count = 0;

# check for bad imports
open(OBJDUMP, "objdump -p build/msys2/BletchMAME.exe|");
while(<OBJDUMP>)
{
	if (/DLL Name: ([^\s]+\.dll)/)
	{
		my $dll = lc $1;
		if (!exists $allowed_dlls_hash{$dll})
		{
			print "FOUND BAD DLL IMPORT: $dll\n";
			$bad_import_count++;
		}
	}
}
close OBJDUMP;

# report on results
if ($bad_import_count > 0)
{
	print "BAD IMPORTS FOUND - STOPPING BUILD\n";
	exit 1;
}
print "No bad imports found\n";
