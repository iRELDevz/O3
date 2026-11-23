$ErrorActionPreference = 'Stop'
$registryPrefix = 'Registry::HKEY_CURRENT_USER\'
$instance = $env:O3_SCAN_INSTANCE
$config = $null
if ($env:O3_SCAN_CONFIG -and (Test-Path -LiteralPath $env:O3_SCAN_CONFIG -PathType Leaf)) {
    try { $config = [IO.File]::ReadAllLines($env:O3_SCAN_CONFIG) } catch {}
}

foreach ($definition in [IO.File]::ReadAllLines((Join-Path $PSScriptRoot 'probes.txt'))) {
    $fields = $definition.Split('|')
    if ($fields.Length -ne 6) { continue }
    $id, $kind, $path, $name, $on, $off = $fields
    try {
        if ($kind -eq 'I' -or $kind -eq 'G') {
            if (-not $config) { continue }
            if ($kind -eq 'I') {
                if (-not $instance) { continue }
                $key = "bst.instance.$instance.$path"
            } else { $key = "bst.$path" }
            $matching = @($config | Where-Object { $_.StartsWith("$key=", [StringComparison]::Ordinal) })
            if ($matching.Length -ne 1 -or $matching[0] -notmatch '="([01])"\s*$') { continue }
            $raw = $Matches[1]
        } else {
            $property = Get-Item -LiteralPath ($registryPrefix + $path)
            $expected = if ($kind -eq 'D') { 'DWord' } else { 'String' }
            if ($property.GetValueKind($name).ToString() -ne $expected) { continue }
            $raw = [string]$property.GetValue($name)
        }
        if ($raw -notmatch '^[0-9]{1,10}$') { continue }
        if ($kind -eq 'B') {
            if (([uint32]$raw -band [uint32]$on) -ne 0) { "$id=1" } else { "$id=0" }
        } elseif ($on.Split(',') -contains $raw) { "$id=1" }
        elseif ($off -eq $raw) { "$id=0" }
    } catch {
        # The parent tries a separate reader for any item with no result.
    }
}
