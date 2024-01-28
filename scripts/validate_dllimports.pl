#!/usr/bin/env perl

#######################################################################################
# validate_dllimports.pl - Validates that the list of dependencies are what we expect #
#######################################################################################

# These are the DLLs we allow
my @allowed_dlls =
(
	"advapi32.dll",
	"api-ms-win-core-synch-l1-2-0.dll",
	"api-ms-win-core-winrt-l1-1-0.dll",
	"api-ms-win-core-winrt-string-l1-1-0.dll",
	"authz.dll",
	"authz.dll",
	"comdlg32.dll",
	"d3d11.dll",
	"d3d12.dll",
	"d3d9.dll",
	"dcomp.dll",
	"dwmapi.dll",
	"dwrite.dll",
	"dxgi.dll",
	"gdi32.dll",
	"imm32.dll",
	"kernel32.dll",
	"msvcrt.dll",
	"netapi32.dll",
	"ole32.dll",
	"oleaut32.dll",
	"rpcrt4.dll",
	"setupapi.dll",
	"shcore.dll",
	"shell32.dll",
	"shlwapi.dll",
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
