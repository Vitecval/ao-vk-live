param([string]$ResultsPath = '')
# Isolated synthetic files only; never inspect or change an installed client.
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../tools/setup-safety.ps1')
$fixture=Join-Path ([IO.Path]::GetTempPath()) ('ao-setup-hash-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $fixture | Out-Null
$results=[ordered]@{result='failed';host=$PSVersionTable.PSEdition;pointerBits=([IntPtr]::Size*8);checks=@()}
function Assert($value,$message){if(!$value){throw $message}}
function Reference-SetupHash([string]$Path){
    Assert-AoSetupPath $Path
    if([IO.Directory]::Exists($Path)){throw "An installer file path is occupied by a folder: $Path."}
    if(![IO.File]::Exists($Path)){return $null}
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}
try {
    $random=[Random]::new(719)
    $paths=@()
    foreach($length in @(0,1,63,64,4095,4096,65535,65536,1048576,4194305)){
        $path=Join-Path $fixture ("literal [$length] # unicode-å.bin")
        $bytes=New-Object byte[] $length;$random.NextBytes($bytes)
        [IO.File]::WriteAllBytes($path,$bytes);$paths+=,$path
        Assert ((Reference-SetupHash $path) -ceq (Get-AoSetupHash $path)) "Production SHA256 differs for $length bytes"
    }
    $results.checks+='sha256_exact_for_empty_small_large_literal_and_unicode_paths'
    Assert ($null -eq (Get-AoSetupHash (Join-Path $fixture 'missing.bin'))) 'Production missing file result changed'
    $refused=$false;try{Get-AoSetupHash $fixture | Out-Null}catch{$refused=$true};Assert $refused 'Production directory accepted'
    $results.checks+='missing_returns_null_directory_refused'
    $locked=[IO.File]::Open($paths[4],[IO.FileMode]::Open,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)
    try{
        $refused=$false;try{Get-AoSetupHash $paths[4] | Out-Null}catch{$refused=$true};Assert $refused 'Production unreadable locked file accepted'
    }finally{$locked.Dispose()}
    $results.checks+='exclusive_file_lock_refused_without_mutation'
    # Linked paths remain prohibited even when their bytes would hash correctly.
    $outside=Join-Path $fixture 'outside';$link=Join-Path $fixture 'linked'
    New-Item -ItemType Directory -Path $outside | Out-Null
    [IO.File]::WriteAllText((Join-Path $outside 'payload.bin'),'must remain untouched')
    New-Item -ItemType Junction -Path $link -Target $outside | Out-Null
    try{
        $script:AoSetupCheckedFolders=@{}
        $refused=$false;try{Get-AoSetupHash (Join-Path $link 'payload.bin') | Out-Null}catch{$refused=$true}
        Assert $refused 'Linked parent accepted'
        Assert ([IO.File]::ReadAllText((Join-Path $outside 'payload.bin')) -ceq 'must remain untouched') 'Linked source changed'
    }finally{[IO.Directory]::Delete($link)}
    $results.checks+='reparse_parent_refused_and_link_target_preserved'
    # Warm both implementations, then alternate order to reduce cache bias.
    $bench=$paths[7]
    for($i=0;$i -lt 8;$i++){Reference-SetupHash $bench | Out-Null;Get-AoSetupHash $bench | Out-Null}
    $refTimes=@();$candidateTimes=@()
    for($round=0;$round -lt 4;$round++){
        foreach($which in $(if($round%2){@('candidate','reference')}else{@('reference','candidate')})){
            $timer=[Diagnostics.Stopwatch]::StartNew()
            for($i=0;$i -lt 128;$i++){if($which -eq 'reference'){Reference-SetupHash $bench | Out-Null}else{Get-AoSetupHash $bench | Out-Null}}
            $timer.Stop()
            if($which -eq 'reference'){$refTimes+=,$timer.Elapsed.TotalMilliseconds}else{$candidateTimes+=,$timer.Elapsed.TotalMilliseconds}
        }
    }
    $results.benchmark=[ordered]@{filesPerRound=128;bytesPerFile=65536;rounds=4;referenceMilliseconds=$refTimes;productionMilliseconds=$candidateTimes;referenceTotalMs=($refTimes|Measure-Object -Sum).Sum;productionTotalMs=($candidateTimes|Measure-Object -Sum).Sum}
    $results.benchmark.speedup=$results.benchmark.referenceTotalMs/$results.benchmark.productionTotalMs
    # Performance is reported, never asserted against a noisy host threshold.
    $results.result='passed'
}catch{$results.error=$_.Exception.Message;throw}
finally{
    if($ResultsPath){$results | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $ResultsPath}
    $results | ConvertTo-Json -Depth 8
    # Resolved, freshly-created test directory only.
    if([IO.Path]::GetFullPath($fixture).StartsWith([IO.Path]::GetFullPath([IO.Path]::GetTempPath()),[StringComparison]::OrdinalIgnoreCase)){
        Remove-Item -LiteralPath $fixture -Recurse -Force
    }
}
