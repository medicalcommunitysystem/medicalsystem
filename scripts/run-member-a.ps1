param([switch]$InitializePassword, [string]$MysqlRoot='D:\sql\MySQL\MySQL Server 8.0')
$ErrorActionPreference='Stop'
$taskRepo=Split-Path -Parent $PSScriptRoot
$taskOriginalPath=$env:PATH
$taskOriginalPassword=[Environment]::GetEnvironmentVariable('MEDICAL_DB_PASSWORD','Process')
$taskSecure=$null
$taskPointer=[IntPtr]::Zero
try {
    Set-Location -LiteralPath $taskRepo
    $env:PATH=(Join-Path $MysqlRoot 'lib')+';'+(Join-Path $MysqlRoot 'bin')+';D:\mingw64\bin;'+$env:PATH
    if ([string]::IsNullOrWhiteSpace($env:MEDICAL_DB_USER)) { $env:MEDICAL_DB_USER=Read-Host 'MySQL user' }
    if ($null -eq $taskOriginalPassword) {
        $taskSecure=Read-Host 'MySQL password (hidden)' -AsSecureString
        $taskPointer=[Runtime.InteropServices.Marshal]::SecureStringToBSTR($taskSecure)
        $env:MEDICAL_DB_PASSWORD=[Runtime.InteropServices.Marshal]::PtrToStringBSTR($taskPointer)
    }
    if ($InitializePassword) { & (Join-Path $taskRepo 'build\medical_system.exe') --initialize-password }
    else { & (Join-Path $taskRepo 'build\medical_system.exe') }
} finally {
    $env:PATH=$taskOriginalPath
    [Environment]::SetEnvironmentVariable('MEDICAL_DB_PASSWORD',$taskOriginalPassword,'Process')
    if ($taskPointer -ne [IntPtr]::Zero) { [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($taskPointer) }
    if ($null -ne $taskSecure) { $taskSecure.Dispose() }
}
