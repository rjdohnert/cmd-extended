param(
    [string]$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
)

$releaseDir = Join-Path $ProjectRoot "Release"
$installerDir = Join-Path $ProjectRoot "installer"
$wxsPath = Join-Path $installerDir "Product.wxs"
$licenseRtfPath = Join-Path $installerDir "License.rtf"
$msiPath = Join-Path $releaseDir "CrossShell-BSD-3.0-x64.msi"

$productName = "CrossShell-BSD"
$manufacturer = "CrossShell"
$productVersion = "3.0.0"
$upgradeCode = "9E40C982-9F69-4EED-9C31-27332A9282C9"

$exeFiles = Get-ChildItem -Path $releaseDir -Filter *.exe | Sort-Object Name
if (-not $exeFiles) {
    throw "No .exe files found in '$releaseDir'."
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

$licenseText = @"
BSD 3-Clause License

Copyright (c) 2026, Roberto J Dohnert
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
"@

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
  <Package Name="$productName" Manufacturer="$manufacturer" Version="$productVersion" UpgradeCode="$upgradeCode" Scope="perMachine">
    <MajorUpgrade DowngradeErrorMessage="A newer version of [ProductName] is already installed." AllowSameVersionUpgrades="yes" />
    <MediaTemplate />
    <Launch Condition="Privileged" Message="Administrator privileges are required to install to C:\Windows\System32." />
    <ui:WixUI Id="WixUI_FeatureTree" />
    <WixVariable Id="WixUILicenseRtf" Value="License.rtf" />

    <Feature Id="MainFeature" Title="$productName" Level="1">
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
