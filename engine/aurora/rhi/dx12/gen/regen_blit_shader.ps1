# Regenerates D3D12BlitShader.h from D3D12Blit.hlsl using dxc.
#
# Usage:
#   powershell -File regen_blit_shader.ps1 -Dxc <path-to-dxc.exe>
#
# The DXIL blobs are embedded so the RHI backend stays independent of the
# shader compiler module at runtime.

param(
    [Parameter(Mandatory = $true)][string]$Dxc
)

$ErrorActionPreference = "Stop"
$root = $PSScriptRoot
$hlsl = Join-Path $root "D3D12Blit.hlsl"
$out  = Join-Path $root "D3D12BlitShader.h"

$vs = Join-Path $env:TEMP "sky_blit_vs.dxil"
$ps = Join-Path $env:TEMP "sky_blit_ps.dxil"

& $Dxc -T vs_6_0 -E VSMain -Fo $vs $hlsl
if ($LASTEXITCODE -ne 0) { throw "vs_6_0 compile failed" }
& $Dxc -T ps_6_0 -E PSMain -Fo $ps $hlsl
if ($LASTEXITCODE -ne 0) { throw "ps_6_0 compile failed" }

function Format-Blob([string]$name, [byte[]]$bytes) {
    $sb = New-Object System.Text.StringBuilder
    [void]$sb.AppendLine("    static const unsigned char $name[] = {")
    for ($i = 0; $i -lt $bytes.Length; $i += 12) {
        $line = "        "
        for ($j = $i; $j -lt [Math]::Min($i + 12, $bytes.Length); ++$j) {
            $line += ("0x{0:X2}, " -f $bytes[$j])
        }
        [void]$sb.AppendLine($line.TrimEnd())
    }
    [void]$sb.AppendLine("    };")
    [void]$sb.AppendLine("")
    return $sb.ToString()
}

$content = New-Object System.Text.StringBuilder
[void]$content.AppendLine("//")
[void]$content.AppendLine("// GENERATED FILE - do not edit.")
[void]$content.AppendLine("// Source: D3D12Blit.hlsl, regenerate with regen_blit_shader.ps1.")
[void]$content.AppendLine("//")
[void]$content.AppendLine("")
[void]$content.AppendLine("#pragma once")
[void]$content.AppendLine("")
[void]$content.AppendLine("namespace sky::aurora::blit {")
[void]$content.AppendLine("")
[void]$content.AppendLine((Format-Blob "kBlitVS" ([System.IO.File]::ReadAllBytes($vs))))
[void]$content.AppendLine((Format-Blob "kBlitPS" ([System.IO.File]::ReadAllBytes($ps))))
[void]$content.AppendLine("} // namespace sky::aurora::blit")

[System.IO.File]::WriteAllText($out, $content.ToString())
Write-Host "Wrote $out"
