param([string]$Revision='36')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$work=Join-Path $root ('build\preview23\atmosphere-r'+$Revision)
$fixture=Join-Path $work ('installer-check-'+[guid]::NewGuid().ToString('N'))
$client=Join-Path $fixture 'client'
$audit=Join-Path $fixture 'audit'
New-Item -ItemType Directory -Path $client,(Join-Path $client 'randy-vk'),(Join-Path $audit 'checks') -Force | Out-Null
foreach($name in @('ao-vk.dll','Anarchy.exe','AnarchyOnline.exe','randy31.dll','ddraw.dll','randy-vk\mesh-ids.bin')) {
    [IO.File]::WriteAllText((Join-Path $client $name),'original-'+$name)
}
$oldHash=(Get-FileHash -LiteralPath (Join-Path $client 'ao-vk.dll') -Algorithm SHA256).Hash
$receipt=@{product='AO-VK Live';clientDirectory=$client;addon=@{'ao-vk.dll'=$oldHash};unrelated='retain-me'}
$receipt | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $client 'ao-vk-live.install.json') -Encoding ASCII
@'
[Renderer]
RVK_AmbientOn=0
RVK_AmbientMotes=1
RVK_AmbientBright=1
RVK_FrameGen=0
UnrelatedSetting=17
[Migrations]
AmbientScale31=1
[Other]
Untouched=23
'@ | Set-Content -LiteralPath (Join-Path $client 'randy-vk.ini') -Encoding ASCII
Copy-Item -LiteralPath (Join-Path $work 'checks\RESULTS.json') -Destination (Join-Path $audit 'checks\RESULTS.json')
if(Test-Path -LiteralPath (Join-Path $work 'stability-checks\RESULTS.json')) {
    New-Item -ItemType Directory -Path (Join-Path $audit 'stability-checks') | Out-Null
    Copy-Item -LiteralPath (Join-Path $work 'stability-checks\RESULTS.json') -Destination (Join-Path $audit 'stability-checks\RESULTS.json')
}
if([int]$Revision -ge 39) {
    foreach($name in @('PAYLOAD.json','ZONING-CHECK.json')){Copy-Item -LiteralPath (Join-Path $work $name) -Destination (Join-Path $audit $name)}
}
if([int]$Revision -ge 40) {
    New-Item -ItemType Directory -Path (Join-Path $audit 'feature-checks') | Out-Null
    Copy-Item -LiteralPath (Join-Path $work 'feature-checks/RESULTS.json') -Destination (Join-Path $audit 'feature-checks/RESULTS.json')
}
$script=Get-Content -LiteralPath (Join-Path $root ('tools\install-preview23-r'+$Revision+'.ps1')) -Raw
# Run the actual installer against dummy files entirely inside the project.
$script=$script.Replace('F:\AO_SInstall\Anarchy Online',$client)
$script=$script.Replace('$root=Split-Path $PSScriptRoot -Parent',('$root='''+$root+''''))
$script=$script.Replace(('$work=Join-Path $root ''build\preview23\atmosphere-r'+$Revision+''''),('$work='''+$audit+''''))
$testScript=Join-Path $fixture 'fixture-installer.ps1'
[IO.File]::WriteAllText($testScript,$script)
& "$env:WINDIR\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File $testScript
if($LASTEXITCODE){throw 'Windows PowerShell installer fixture failed.'}
$result=Get-Content -LiteralPath (Join-Path $audit 'INSTALL-RESULTS.json') -Raw | ConvertFrom-Json
if($result.result -ne 'passed' -or $result.client -ne $client){throw 'Installer result mismatch.'}
$expected=(Get-Content -LiteralPath (Join-Path $work 'checks\RESULTS.json') -Raw | ConvertFrom-Json).rendererSha256
if((Get-FileHash -LiteralPath (Join-Path $client 'ao-vk.dll') -Algorithm SHA256).Hash -ine $expected){throw 'Renderer mismatch.'}
$ini=[IO.File]::ReadAllText((Join-Path $client 'randy-vk.ini'))
foreach($line in @('RVK_AmbientMotes=2','RVK_AmbientBright=1.5','RVK_ImprovedTiles=0','RVK_SkyClouds=1','RVK_SkyTraffic=1','RVK_CloudsOn=0','UnrelatedSetting=17','Untouched=23')) {
    if($ini -notmatch ('(?m)^'+[regex]::Escape($line)+'\r?$')){throw "Missing or damaged INI value: $line"}
}
if([int]$Revision -ge 39) {
    if($ini -notmatch '(?m)^RVK_ZoneScreen=1\r?$'){throw 'Zoning default not installed.'}
    $manifest=Get-Content -LiteralPath (Join-Path $work 'PAYLOAD.json') -Raw | ConvertFrom-Json
    foreach($entry in $manifest.files.PSObject.Properties){
        if((Get-FileHash -LiteralPath (Join-Path $client $entry.Name) -Algorithm SHA256).Hash -ine $entry.Value){throw ('Installed payload mismatch: '+$entry.Name)}
    }
}
$receipt=Get-Content -LiteralPath (Join-Path $client 'ao-vk-live.install.json') -Raw | ConvertFrom-Json
if($receipt.unrelated -ne 'retain-me'){throw 'Unrelated receipt data changed.'}
@{result='passed';scope='Actual installer in Windows PowerShell 5.1 against project-local dummy client; F: untouched';fixture=$fixture;rendererSha256=$expected} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $work 'INSTALLER-CHECK.json') -Encoding UTF8
Write-Output 'Windows PowerShell 5.1 full installer check passed; settings and receipt retained.'
