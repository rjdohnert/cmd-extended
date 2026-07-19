param(
    [string]$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
)

$releaseDir = Join-Path $ProjectRoot "Release"
$installerDir = Join-Path $ProjectRoot "installer"
$wxsPath = Join-Path $installerDir "Product.wxs"
$licenseTxtPath = Join-Path $ProjectRoot "LICENSE.txt"
$licenseRtfPath = Join-Path $installerDir "License.rtf"
$msiPath = Join-Path $releaseDir "cmd-extended-installer.msi"

$requiredExe = Join-Path $releaseDir "htop.exe"
if (-not (Test-Path -Path $requiredExe -PathType Leaf)) {
  throw "Required executable missing: '$requiredExe'. Build aborted."
}

$exeFiles = Get-ChildItem -Path $releaseDir -Filter *.exe | Sort-Object Name
if (-not $exeFiles) {
    throw "No .exe files found in '$releaseDir'."
}

if (-not (Test-Path -Path $licenseTxtPath -PathType Leaf)) {
  throw "License text file not found: '$licenseTxtPath'."
}

function Convert-ToRtf {
  param(
    [Parameter(Mandatory = $true)]
    [string]$Text
  )

  $escaped = $Text.Replace('\', '\\').Replace('{', '\{').Replace('}', '\}')
  $escaped = $escaped -replace "`r`n", "`n"
  $escaped = $escaped -replace "`r", "`n"
  $escaped = $escaped -replace "`n", "\par`r`n"

  return "{\rtf1\ansi\deff0{\fonttbl{\f0 Consolas;}}\fs20`r`n$escaped`r`n}"
}

$licenseText = Get-Content -Path $licenseTxtPath -Raw
$licenseRtf = Convert-ToRtf -Text $licenseText
Set-Content -Path $licenseRtfPath -Value $licenseRtf -Encoding ASCII

function New-WixId {
    param(
        [string]$Name,
        [int]$Index
    )

    $base = [System.IO.Path]::GetFileNameWithoutExtension($Name) -replace '[^A-Za-z0-9_]', '_'
    if ($base -match '^[0-9]') {
        $base = "F_$base"
    }

    return "Cmp_${base}_${Index}"
}

$componentXml = New-Object System.Collections.Generic.List[string]

for ($i = 0; $i -lt $exeFiles.Count; $i++) {
    $fileName = $exeFiles[$i].Name
    $componentId = New-WixId -Name $fileName -Index $i

  $componentXml.Add(('      <Component Id="{0}" Guid="*" Bitness="always64">' -f $componentId))
  $componentXml.Add(('        <File Source="..\Release\{0}" KeyPath="yes" />' -f $fileName))
  $componentXml.Add('      </Component>')

}

$wxs = @"
<?xml version="1.0" encoding="UTF-8"?>
<Wix xmlns="http://wixtoolset.org/schemas/v4/wxs" xmlns:ui="http://wixtoolset.org/schemas/v4/wxs/ui">
  <Package Name="cmd-extended" Manufacturer="cmd-extended" Version="1.0.0" UpgradeCode="9E40C982-9F69-4EED-9C31-27332A9282C9" Scope="perMachine">
    <MajorUpgrade DowngradeErrorMessage="A newer version of [ProductName] is already installed." AllowSameVersionUpgrades="yes" />
    <MediaTemplate />
    <Launch Condition="Privileged" Message="Administrator privileges are required to install to C:\Windows\System32." />
    <ui:WixUI Id="WixUI_FeatureTree" />
    <WixVariable Id="WixUILicenseRtf" Value="License.rtf" />

    <Feature Id="MainFeature" Title="cmd-extended" Level="1">
      <ComponentGroupRef Id="CmdExtendedExecutables" />
    </Feature>
  </Package>

  <Fragment>
    <ComponentGroup Id="CmdExtendedExecutables" Directory="System64Folder">
$(($componentXml -join "`r`n"))
    </ComponentGroup>
  </Fragment>
</Wix>
"@

Set-Content -Path $wxsPath -Value $wxs -Encoding UTF8

Push-Location $installerDir
try {
    wix build .\Product.wxs -arch x64 -ext WixToolset.UI.wixext -o $msiPath
  if ($LASTEXITCODE -ne 0) {
    throw "WiX build failed with exit code $LASTEXITCODE."
  }
}
finally {
    Pop-Location
}

Write-Host "Generated WiX source: $wxsPath"
Write-Host "Generated license RTF: $licenseRtfPath"
Write-Host "Built MSI: $msiPath"
