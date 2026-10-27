param([string]$BuildDir='build/windows-live', [string]$OutputDir='build/groundplus-fix')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$exe=Join-Path $root "$BuildDir/rvk_demo.exe"
$out=Join-Path $root $OutputDir
$env:RANDYVK_PIPELINE_CACHE=Join-Path $root 'build/shared-terrain-check/pipeline.cache'
$common=@('--native-ground-test','--ground-detail-fixture','--terrain-maps','--pixel-lighting','--sky-zone','655','--fixed-clock','1000')
$cases=@(
 @{Name='groundon';Args=@('--native-on','--terrain-blend','0')},
 @{Name='shared';Args=@('--terrain-blend','1')},
 @{Name='shared-groundon';Args=@('--native-on','--terrain-blend','1')},
 @{Name='plus';Args=@('--terrain-blend','2')},
 @{Name='plus-manual';Args=@('--native-on','--terrain-blend','2')},
 @{Name='plus-reference';Args=@('--native-on','--terrain-blend','1','--ground-ao','0','--ground-sun','0','--dune-light','0','--ground-blend','0')},
 @{Name='plus-no-edge-blend';Args=@('--terrain-blend','2','--ground-blend','0')},
 @{Name='plus-original-textures';Args=@('--native-on','--terrain-blend','0','--ground-ao','0','--ground-sun','0','--dune-light','0','--ground-blend','0')},
 @{Name='plus-no-pebbles';Args=@('--terrain-blend','2','--ground-pebbles','0')},
 @{Name='cliff-on';Args=@('--terrain-blend','2','--ground-pebbles','0','--ground-rocks','0')},
 @{Name='cliff-off';Args=@('--terrain-blend','2','--ground-pebbles','0','--ground-rocks','0','--terrain-off')},
 @{Name='cycle';Args=@('--terrain-blend','2','--ground-mode-cycle')},
 @{Name='edge-plus';Args=@('--terrain-yaw','-1.5708','--terrain-blend','2')},
 @{Name='edge-no-blend';Args=@('--terrain-yaw','-1.5708','--native-on','--terrain-blend','1','--ground-ao','0','--ground-sun','0','--dune-light','0','--ground-blend','0')},
 @{Name='edge-with-blend';Args=@('--terrain-yaw','-1.5708','--native-on','--terrain-blend','1','--ground-ao','0','--ground-sun','0','--dune-light','0','--ground-blend','1')},
 @{Name='separate-plus';Separate=$true;Args=@('--terrain-blend','2')},
 @{Name='separate-reference';Separate=$true;Args=@('--native-on','--terrain-blend','1','--ground-ao','0','--ground-sun','0','--dune-light','0','--ground-blend','0')},
 @{Name='regression-continuity-flat';Shared=$true;Args=@('--terrain-blend','1')},
 @{Name='regression-road-rough';Shared=$true;Args=@('--terrain-id','180','--terrain-hd','1','--terrain-blend','1','--pbr-debug','2')},
 @{Name='regression-deck-palette';Shared=$true;Args=@('--terrain-id','17','--terrain-authored','--terrain-blend','1','--terrain-palette-control')}
)
foreach($case in $cases) {
 $dir=Join-Path $out $case.Name
 New-Item -ItemType Directory -Force $dir | Out-Null
 $base=if($case.Shared){@('--shared-terrain-test','--frames','80')}else{@($common)+@('--frames','480')}
 if(!$case.Shared -and !$case.Separate){$base+=@('--grass-combined')}
 Push-Location $dir
 try {
  & $exe @base --size 768 512 --shot result.bmp @($case.Args) *> run.txt
  if($LASTEXITCODE -ne 0){throw "Renderer test failed: $($case.Name)"}
  Write-Output "$($case.Name) passed"
 } finally {Pop-Location}
}
