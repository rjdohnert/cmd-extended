param(
    [string]$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
)

$releaseDir = Join-Path $ProjectRoot "Release"
$installerDir = Join-Path $ProjectRoot "installer"
$wxsPath = Join-Path $installerDir "Product.wxs"
$msiPath = Join-Path $releaseDir "cmd-extended-installer.msi"

$exeFiles = Get-ChildItem -Path $releaseDir -Filter *.exe | Sort-Object Name
if (-not $exeFiles) {
    throw "No .exe files found in '$releaseDir'."
}

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

  $componentXml.Add(('      <Component Id="{0}" Guid="*">' -f $componentId))
  $componentXml.Add(('        <File Source="..\Release\{0}" KeyPath="yes" />' -f $fileName))
  $componentXml.Add('      </Component>')

}

$wxs = @"
<?xml version="1.0" encoding="UTF-8"?>
<Wix xmlns="http://wixtoolset.org/schemas/v4/wxs">
  <Package Name="cmd-extended" Manufacturer="cmd-extended" Version="1.0.0" UpgradeCode="9E40C982-9F69-4EED-9C31-27332A9282C9" Scope="perMachine">
    <MajorUpgrade DowngradeErrorMessage="A newer version of [ProductName] is already installed." AllowSameVersionUpgrades="yes" />
    <MediaTemplate />
    <Launch Condition="Privileged" Message="Administrator privileges are required to install to C:\Windows\System32." />

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
    wix build .\Product.wxs -arch x64 -o $msiPath
  if ($LASTEXITCODE -ne 0) {
    throw "WiX build failed with exit code $LASTEXITCODE."
  }
}
finally {
    Pop-Location
}

Write-Host "Generated WiX source: $wxsPath"
Write-Host "Built MSI: $msiPath"
