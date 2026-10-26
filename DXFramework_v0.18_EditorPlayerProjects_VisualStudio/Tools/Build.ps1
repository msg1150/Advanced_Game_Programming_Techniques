#requires -Version 7.0
param(
    [ValidateSet('Debug','Release')][string]$Configuration='Debug',
    [string]$OutputDirectory='',
    [string]$Target='DXFramework.sln',
    [switch]$NoPdbWorkaround
)
$ErrorActionPreference='Stop'
$sourceRoot=Split-Path $PSScriptRoot -Parent
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vsRoot=(& $vswhere -version '[17.0,18.0)' -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1)
if(!$vsRoot){$vsRoot=(& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath|Select-Object -First 1)}
if(!$vsRoot){throw 'MSVC installation not found'}
if(!$OutputDirectory){$OutputDirectory=Join-Path $sourceRoot "bin/x64/$Configuration"}
$OutputDirectory=[IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Path $OutputDirectory -Force|Out-Null
$start=[Diagnostics.ProcessStartInfo]::new()
$start.FileName=Join-Path $vsRoot 'MSBuild/Current/Bin/MSBuild.exe'
$start.WorkingDirectory=$sourceRoot;$start.UseShellExecute=$false;$start.CreateNoWindow=$true
foreach($argument in @((Join-Path $sourceRoot $Target),"/p:Configuration=$Configuration",'/p:Platform=x64',"/p:OutDir=$($OutputDirectory.TrimEnd('/','\'))/",'/m:1','/nr:false','/nologo','/verbosity:minimal'))
{$start.ArgumentList.Add($argument)}
if($NoPdbWorkaround){$start.Environment['_CL_']='/Z7';$start.Environment['_LINK_']='/DEBUG:NONE /INCREMENTAL:NO'}
# FileName/ArgumentList are passed to CreateProcess by .NET; no cmd /c or string-built shell command.
$process=[Diagnostics.Process]::Start($start);$process.WaitForExit()
if($process.ExitCode -ne 0){throw "Build failed: $($process.ExitCode)"}
Write-Output "BUILD PASS: $OutputDirectory"
