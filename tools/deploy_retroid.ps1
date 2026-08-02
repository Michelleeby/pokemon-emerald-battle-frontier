param(
    [Parameter(Mandatory = $true)]
    [string]$RomPath
)

$ErrorActionPreference = "Stop"

$deviceName = "Retroid Pocket Classic"
$storageName = "Internal shared storage"
$romFileName = "Pokemon - Emerald Version - Battle Frontier.gba"
$copyFlags = 0x614 # Silent, no confirmation, no error UI.
$timeoutSeconds = 120

function Get-ChildItemByName {
    param(
        [Parameter(Mandatory = $true)]$Folder,
        [Parameter(Mandatory = $true)][string]$Name
    )

    return $Folder.Items() |
        Where-Object { $_.Name -eq $Name } |
        Select-Object -First 1
}

function Get-RequiredFolder {
    param(
        [Parameter(Mandatory = $true)]$ParentFolder,
        [Parameter(Mandatory = $true)][string]$Name
    )

    $item = Get-ChildItemByName -Folder $ParentFolder -Name $Name
    if ($null -eq $item -or -not $item.IsFolder) {
        throw "Could not find folder '$Name' on the connected Retroid."
    }

    $folder = $item.GetFolder
    if ($null -eq $folder) {
        throw "Could not open folder '$Name' on the connected Retroid."
    }

    return $folder
}

if (-not (Test-Path -LiteralPath $RomPath -PathType Leaf)) {
    throw "Built ROM not found: $RomPath"
}

$sourceHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $RomPath).Hash
$stagingDirectory = Join-Path ([System.IO.Path]::GetTempPath()) ("pokeemerald-retroid-stage-" + [guid]::NewGuid())
$verificationRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("pokeemerald-retroid-verify-" + [guid]::NewGuid())

New-Item -ItemType Directory -Path $stagingDirectory | Out-Null
New-Item -ItemType Directory -Path $verificationRoot | Out-Null

try {
    $stagedRom = Join-Path $stagingDirectory $romFileName
    Copy-Item -LiteralPath $RomPath -Destination $stagedRom

    $shell = New-Object -ComObject Shell.Application
    $thisPc = $shell.Namespace(17)
    $device = Get-ChildItemByName -Folder $thisPc -Name $deviceName
    if ($null -eq $device -or -not $device.IsFolder) {
        throw "The Retroid Pocket Classic is not connected or unlocked."
    }

    $deviceFolder = $device.GetFolder
    $storageFolder = Get-RequiredFolder -ParentFolder $deviceFolder -Name $storageName
    $romsFolder = Get-RequiredFolder -ParentFolder $storageFolder -Name "ROMs"
    $gbaFolder = Get-RequiredFolder -ParentFolder $romsFolder -Name "gba"

    Write-Host "Deploying $romFileName to $deviceName..."
    $gbaFolder.CopyHere($stagedRom, $copyFlags)

    $deadline = (Get-Date).AddSeconds($timeoutSeconds)
    $verified = $false
    while ((Get-Date) -lt $deadline -and -not $verified) {
        Start-Sleep -Seconds 2
        $remoteRom = $gbaFolder.Items() |
            Where-Object { $_.Name -eq $romFileName -or $_.Name -eq [System.IO.Path]::GetFileNameWithoutExtension($romFileName) } |
            Select-Object -First 1
        if ($null -eq $remoteRom) {
            continue
        }

        $attemptDirectory = Join-Path $verificationRoot ([guid]::NewGuid().ToString())
        New-Item -ItemType Directory -Path $attemptDirectory | Out-Null
        $localVerificationFolder = $shell.Namespace($attemptDirectory)
        $localVerificationFolder.CopyHere($remoteRom, $copyFlags)

        $attemptDeadline = (Get-Date).AddSeconds(15)
        while ((Get-Date) -lt $attemptDeadline) {
            Start-Sleep -Milliseconds 500
            $copiedRom = Get-ChildItem -LiteralPath $attemptDirectory -File -ErrorAction SilentlyContinue |
                Select-Object -First 1
            if ($null -eq $copiedRom -or $copiedRom.Length -ne (Get-Item -LiteralPath $RomPath).Length) {
                continue
            }

            if ((Get-FileHash -Algorithm SHA256 -LiteralPath $copiedRom.FullName).Hash -eq $sourceHash) {
                $verified = $true
            }
            break
        }
    }

    if (-not $verified) {
        throw "The Retroid transfer could not be verified within $timeoutSeconds seconds."
    }

    Write-Host "Retroid deployment verified (SHA-256 $sourceHash)."
}
finally {
    Remove-Item -LiteralPath $stagingDirectory -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $verificationRoot -Recurse -Force -ErrorAction SilentlyContinue
}
