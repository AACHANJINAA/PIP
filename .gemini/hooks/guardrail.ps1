# 1. 입력 JSON 읽기
$inputJson = $Input | Out-String
if (-not $inputJson) { exit 0 }

# 2. 파일 경로와 새 내용 추출 (jq 활용)
$filePath = $inputJson | jq -r '.tool_input.path //.tool_input.file_path'
$newContent = $inputJson | jq -r '.tool_input.content //.tool_input.new_string'

# 3. Diff 생성 및 출력 (모든 출력은 [Console]::Error로 보내야 함)
if (Test-Path $filePath) {
    # 에러가 났던 부분 수정
    $tempFile = [System.IO.Path]::GetTempFileName()
    $newContent | Set-Content -Path $tempFile -Encoding utf8
    
    [Console]::Error.WriteLine("`n[변경 사항 검토: $filePath]")
    git diff --no-index --color=always $filePath $tempFile | Out-String | ForEach-Object { [Console]::Error.WriteLine($_) }
    
    Remove-Item $tempFile
} else {
    [Console]::Error.WriteLine("`n[새 파일 생성: $filePath]")
}

# 4. 사용자 승인 (gum 사용, stderr 환경에서 실행)
$approved = powershell.exe -Command "gum confirm '이 수정을 승인하시겠습니까?' --default=true" 2>&1
if ($LASTEXITCODE -ne 0) {
    # 거부 시 JSON 출력 (stdout)
    @{
        decision = "deny"
        reason = "사용자가 수정을 거부했습니다."
        systemMessage = "Gated modification: User rejected the change."
    } | ConvertTo-Json -Compress | Write-Output
    exit 0
}

# 5. 승인 시: 서명 있는 UTF-8(BOM)로 강제 저장
$newContent | Set-Content -Path $filePath -Encoding utf8BOM

# 6. 허용 신호 전송 (stdout)
@{ decision = "allow" } | ConvertTo-Json -Compress | Write-Output