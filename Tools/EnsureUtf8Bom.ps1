# C++ 소스(*.cpp, *.h)를 UTF-8 BOM으로 맞춘다. 필요할 때 직접 실행한다.
# (서버/클라이언트 모두 /utf-8로 컴파일하므로 BOM이 없어도 빌드는 되지만, .editorconfig 규칙에 맞추는 용도)
# - 지정한 폴더 바로 아래 파일만 대상 (imgui 등 하위 폴더의 외부 라이브러리는 건드리지 않음)
# - BOM이 이미 있으면 그대로 두므로 한 번 변환한 뒤로는 아무것도 바꾸지 않는다
# - BOM 없는 UTF-8은 BOM만 붙이고, UTF-8로 읽히지 않는 파일은 CP949로 보고 UTF-8 BOM으로 변환
# 사용: powershell -NoProfile -ExecutionPolicy Bypass -File Tools\EnsureUtf8Bom.ps1 Server\Server Client\Client Common

$ErrorActionPreference = 'Stop'

$bom = [byte[]](0xEF, 0xBB, 0xBF)
$strictUtf8 = New-Object System.Text.UTF8Encoding($false, $true)
$cp949 = [System.Text.Encoding]::GetEncoding(949)
$changed = 0

foreach ($dir in $args)
{
    if (-not (Test-Path -LiteralPath $dir -PathType Container)) { continue }

    foreach ($file in Get-ChildItem -LiteralPath $dir -File | Where-Object { $_.Extension -in '.cpp', '.h' })
    {
        $bytes = [System.IO.File]::ReadAllBytes($file.FullName)
        if ($bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF) { continue }

        try
        {
            [void]$strictUtf8.GetString($bytes)
            $body = $bytes
            $kind = 'UTF-8'
        }
        catch
        {
            $body = [System.Text.Encoding]::UTF8.GetBytes($cp949.GetString($bytes))
            $kind = 'CP949'
        }

        [System.IO.File]::WriteAllBytes($file.FullName, [byte[]]($bom + $body))
        Write-Host "[EnsureUtf8Bom] $kind -> UTF-8 BOM: $($file.FullName)"
        $changed++
    }
}

if ($changed -gt 0) { Write-Host "[EnsureUtf8Bom] $changed file(s) converted" }
exit 0
