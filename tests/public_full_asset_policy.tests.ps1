param(
    [Parameter(Mandatory=$true)][string]$PublishedManifest,
    [Parameter(Mandatory=$true)][string]$PublishedSafety,
    [Parameter(Mandatory=$true)][string]$FixtureRoot,
    [Parameter(Mandatory=$true)][string]$ResultsPath
)
# Dummy installation only: no game binaries, downloads, GPU or real client paths.
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$installer = Join-Path $repo 'tools/install-live.ps1'
$results = [ordered]@{ result='failed'; host=$PSVersionTable.PSEdition; pointerBits=([IntPtr]::Size*8); checks=@() }
function Assert($Value,[string]$Message) { if (!$Value) { throw "ASSERT: $Message" } }
function Check([string]$Name) { $results.checks += $Name }
function Hash([string]$Path) { (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
function Write-Fixture([string]$Path,[string]$Text) {
    New-Item -ItemType Directory -Force (Split-Path -Parent $Path) | Out-Null
    [IO.File]::WriteAllText($Path,$Text)
}
function Reject($Arguments,[string]$Expected) {
    $caught=$false
    try { & $installer @Arguments | Out-Null }
    catch { $caught=$true; Assert ($_.Exception.Message -like ('*'+$Expected+'*')) ('Unexpected failure: '+$_.Exception.Message) }
    Assert $caught ('Expected refusal: '+$Expected)
}
# Installer process discovery is isolated from all actual running clients.
function global:Get-Process { param($Name,$ErrorAction) }
$global:PublicFullFailTarget=''
function global:Move-Item {
    param($LiteralPath,$Destination)
    if ($Destination -eq $global:PublicFullFailTarget -and $LiteralPath -like '*.installing-*') { throw 'public Full fixture commit failure' }
    Microsoft.PowerShell.Management\Move-Item -LiteralPath $LiteralPath -Destination $Destination
}
$savedAppData=$env:APPDATA
try {
    $published=Get-Content -LiteralPath $PublishedManifest -Raw | ConvertFrom-Json
    $paths=@($published.assets.PSObject.Properties.Name)
    Assert ($published.edition -ceq 'full') 'Expected real Full manifest'
    Assert ($paths.Count -gt 10000) 'Full manifest unexpectedly small'
    . $PublishedSafety
    $oldRejected=@($paths | Where-Object { !(Test-AoSetupAssetPath $_) })
    $characters=@($paths | Where-Object { $_ -clike 'randy-vk/packs/ao-reworked-characters/*' })
    $results.fullAssetCount=$paths.Count
    $results.oldRejectedCount=$oldRejected.Count
    $results.oldRejectedOutsideCharacters=@($oldRejected | Where-Object { $_ -cnotlike 'randy-vk/packs/ao-reworked-characters/*' })
    Assert ($characters.Count -eq 1946) 'Published character inventory changed; inspect new release before updating this regression'
    Assert ($oldRejected.Count -eq ($characters.Count+1)) 'Published baseline rejection inventory changed'
    Assert ($results.oldRejectedOutsideCharacters.Count -eq 1 -and $results.oldRejectedOutsideCharacters[0] -ceq 'randy-vk/packs/ao-penumbra-078/CREDITS.txt') 'Unexpected baseline rejection'
    . (Join-Path $repo 'tools/setup-safety.ps1')
    $rejected=@($paths | Where-Object { !(Test-AoSetupAssetPath $_) })
    Assert ($rejected.Count -eq 0) ('Patched policy rejects published assets: '+(($rejected | Select-Object -First 5) -join ', '))
    $results.fullAssetCount=$paths.Count
    $results.oldRejectedCount=$oldRejected.Count
    $results.characterPngCount=@($characters | Where-Object { $_ -clike '*.png' }).Count
    Check 'all_real_published_Full_asset_paths_accepted'
    $invalid=@(
        'randy-vk/packs/ao-reworked-characters/../ao-vk.dll',
        'randy-vk/packs/ao-reworked-characters/source-art/1010004_1.png',
        'randy-vk/packs/ao-reworked-characters/sub/1010004_1.png',
        'randy-vk/packs/ao-reworked-characters/evil.dll',
        'randy-vk/packs/ao-reworked-characters/unlisted.txt',
        'randy-vk/packs/ao-reworked-characters/unlisted.json',
        'randy-vk/packs/ao-penumbra-078/unlisted.txt',
        'randy-vk/packs/ao-reworked-characters/README.MD',
        'randy-vk/packs/ao-reworked-characters/a.png.exe',
        'randy-vk/packs/ao-reworked-characters/a.png:alternate',
        'randy-vk/packs/ao-reworked-characters/a.png/extra',
        'randy-vk/packs/ao-reworked-characters/a b.png',
        'randy-vk/packs/unknown/1010004_1.png',
        'randy-vk\packs\ao-reworked-characters\1010004_1.png',
        '/randy-vk/packs/ao-reworked-characters/1010004_1.png',
        'C:/randy-vk/packs/ao-reworked-characters/1010004_1.png'
    )
    foreach ($path in $invalid) { Assert (!(Test-AoSetupAssetPath $path)) ('Unsafe or unlisted path accepted: '+$path) }
    $results.rejectedInvalidPaths=$invalid.Count
    Check 'new_namespace_stays_flat_exact_and_type_restricted'

    $client=Join-Path $FixtureRoot 'AO folder with spaces'
    $package=Join-Path $FixtureRoot 'package'
    New-Item -ItemType Directory -Force $client,$package | Out-Null
    $env:APPDATA=Join-Path $FixtureRoot 'AppData'
    $originals=@{}
    foreach ($name in @('randy31.dll','DisplaySystem.dll','Gamecode.dll','GUI.dll','N3.dll','FXS.dll')) {
        Write-Fixture (Join-Path $client $name) ('dummy stock '+$name)
        $originals[$name]=Hash (Join-Path $client $name)
    }
    Write-Fixture (Join-Path $client 'AnarchyOnline.exe') 'dummy, never executable'
    Write-Fixture (Join-Path $client 'randy-vk.ini') "[User]`r`nPreserve=1"
    $iniHash=Hash (Join-Path $client 'randy-vk.ini')
    Write-Fixture (Join-Path $package 'ao-vk.dll') 'dummy addon one'
    Write-Fixture (Join-Path $package 'ddraw.dll') 'dummy loader one'
    $assets=@{}
    # Every payload name and hash shape comes from the actual Full manifest;
    # contents are small synthetic bytes and pins are recomputed for the fixture.
    $envMap=$paths | Where-Object { $_ -cmatch '^randy-vk/packs/ao-penumbra-078/[^/]+\.png$' } | Select-Object -First 1
    $charMap=$characters | Where-Object { $_ -clike '*.png' } | Select-Object -First 1
    $subset=@($envMap,$charMap,'randy-vk/packs/ao-penumbra-078/CREDITS.txt')+@($characters | Where-Object { $_ -cnotlike '*.png' })
    Assert ($subset.Count -eq 9) 'Fixture must cover both PNG namespaces, environment credits and six character metadata files'
    foreach ($relative in $subset) {
        Write-Fixture (Join-Path $package $relative) ('dummy published asset '+$relative)
        $assets[$relative]=Hash (Join-Path $package $relative)
    }
    $manifest=@{ product='AO-VK Live'; version=$published.version; edition='full'; build='public078-partial-fixture-one'; client=$originals; addon=@{ 'ao-vk.dll'=(Hash (Join-Path $package 'ao-vk.dll')); 'ddraw.dll'=(Hash (Join-Path $package 'ddraw.dll')) }; assets=$assets; previousAddons=@() }
    $manifestPath=Join-Path $package 'manifest.json'
    $manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $manifestPath
    $argsInstall=@{ ClientDir=$client; PackageDir=$package }
    Write-Fixture (Join-Path $client 'ddraw.dll') 'unknown wrapper, keep exact'
    $wrapperHash=Hash (Join-Path $client 'ddraw.dll')
    Reject $argsInstall 'already exists'
    Reject (@{ ClientDir=$client; PackageDir=$package; Upgrade=$true }) 'unrecognized graphics wrapper'
    Assert ((Hash (Join-Path $client 'ddraw.dll')) -ceq $wrapperHash) 'Unknown wrapper changed'
    Assert (!(Test-Path -LiteralPath (Join-Path $client 'ao-vk.dll'))) 'Wrapper refusal partially installed renderer'
    foreach ($relative in $subset) { Assert (!(Test-Path -LiteralPath (Join-Path $client $relative))) 'Wrapper refusal partially installed artwork' }
    Check 'fresh_and_upgrade_refuse_unknown_wrapper_without_mutation'
    Remove-Item -LiteralPath (Join-Path $client 'ddraw.dll')
    & $installer @argsInstall | Out-Null
    $receiptPath=Join-Path $client 'ao-vk-live.install.json'
    $receipt=Get-Content -LiteralPath $receiptPath -Raw | ConvertFrom-Json
    foreach ($relative in $subset) {
        Assert ((Hash (Join-Path $client $relative)) -ceq $assets[$relative]) ('Installed payload mismatch '+$relative)
        Assert ($receipt.assets.$relative -ceq $assets[$relative]) ('Missing receipt ownership '+$relative)
    }
    Check 'partial_Full_installs_both_namespaces_and_all_character_metadata'
    $oldReceipt=Hash $receiptPath
    $oldAddon=Hash (Join-Path $client 'ao-vk.dll')
    foreach ($relative in $subset) {
        Write-Fixture (Join-Path $package $relative) ('dummy upgrade '+$relative)
        $assets[$relative]=Hash (Join-Path $package $relative)
    }
    Write-Fixture (Join-Path $package 'ao-vk.dll') 'dummy addon two'
    $manifest.addon.'ao-vk.dll'=Hash (Join-Path $package 'ao-vk.dll')
    $manifest.build='public078-partial-fixture-two'
    $manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $manifestPath
    $global:PublicFullFailTarget=Join-Path $client $charMap
    Reject (@{ ClientDir=$client; PackageDir=$package; Upgrade=$true }) 'public Full fixture commit failure'
    $global:PublicFullFailTarget=''
    Assert ((Hash $receiptPath) -ceq $oldReceipt) 'Failed transaction changed receipt'
    Assert ((Hash (Join-Path $client 'ao-vk.dll')) -ceq $oldAddon) 'Failed transaction changed renderer'
    foreach ($relative in $subset) { Assert ((Hash (Join-Path $client $relative)) -ceq $receipt.assets.$relative) ('Failed transaction did not restore '+$relative) }
    Assert (!(Test-Path -LiteralPath (Join-Path $client '.ao-vk-setup-transaction.json'))) 'Rollback journal retained unexpectedly'
    Check 'injected_character_commit_failure_rolls_back_all_owned_payloads'
    & $installer -ClientDir $client -PackageDir $package -Upgrade | Out-Null
    foreach ($relative in $subset) { Assert ((Hash (Join-Path $client $relative)) -ceq $assets[$relative]) ('Upgrade payload mismatch '+$relative) }
    Check 'recognized_upgrade_commits_both_namespaces'
    Write-Fixture (Join-Path $client 'ddraw.dll') 'later unrelated wrapper, preserve'
    $wrapperHash=Hash (Join-Path $client 'ddraw.dll')
    Write-Fixture (Join-Path $client $charMap) 'personal modified character painting'
    $personalHash=Hash (Join-Path $client $charMap)
    & $installer -ClientDir $client -PackageDir $package -Uninstall | Out-Null
    Assert ((Hash (Join-Path $client 'ddraw.dll')) -ceq $wrapperHash) 'Uninstall changed unknown wrapper'
    Assert ((Hash (Join-Path $client $charMap)) -ceq $personalHash) 'Uninstall changed personal artwork'
    foreach ($relative in $subset) { if ($relative -cne $charMap) { Assert (!(Test-Path -LiteralPath (Join-Path $client $relative))) ('Uninstall left owned asset '+$relative) } }
    Assert (!(Test-Path -LiteralPath (Join-Path $client 'ao-vk.dll'))) 'Uninstall left owned renderer'
    Assert ((Hash (Join-Path $client 'randy-vk.ini')) -ceq $iniHash) 'Saved choices changed'
    foreach ($name in $originals.Keys) { Assert ((Hash (Join-Path $client $name)) -ceq $originals[$name]) ('Original client changed '+$name) }
    Check 'uninstall_removes_owned_assets_preserves_unknown_wrapper_personal_art_and_game'
    $results.fixtureAssetCount=$subset.Count
    $results.result='passed'
} catch {
    $results.error=$_.Exception.Message
    throw
} finally {
    $env:APPDATA=$savedAppData
    $results | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $ResultsPath
    Remove-Item Function:\Get-Process,Function:\Move-Item -ErrorAction SilentlyContinue
}
