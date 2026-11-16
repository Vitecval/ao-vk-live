param([switch]$Tests,[string]$Output='')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
if(!$Output){$Output=Join-Path $root 'build/horizon-preview/minimal/Rubi-Ka Horizon Setup.exe'}
$msbuild='C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe'
if(!(Test-Path -LiteralPath $msbuild)){throw 'MSBuild with .NET Framework 4.8 and WPF build tools is required.'}
Push-Location -LiteralPath $root
try {
 New-Item -ItemType Directory -Path build/horizon-preview -Force | Out-Null
 python tools/prepare-horizon-assets.py
 if($LASTEXITCODE){throw 'Horizon asset preparation failed.'}
 & $msbuild apps/horizon/Horizon.csproj /nologo /m:4 /p:Configuration=Release /verbosity:minimal *> build/horizon-preview/build.log
 if($LASTEXITCODE){Get-Content build/horizon-preview/build.log -Tail 20;throw 'Horizon build failed.'}
 python tools/package-horizon.py --output $Output
 if($LASTEXITCODE){throw 'Horizon packaging failed.'}
 if($Tests){
  $exe=[IO.Path]::GetFullPath($Output)
  $state=Join-Path $root ('build/horizon-preview/test-'+[guid]::NewGuid().ToString('N'))
  $p=Start-Process -FilePath $exe -ArgumentList @('--self-test','--state-root',('"'+$state+'"')) -WindowStyle Hidden -Wait -PassThru
  if($p.ExitCode){throw "Horizon settings checks failed. See $state"}
  Write-Output "Horizon settings checks passed: $state"
 }
}finally {Pop-Location}
