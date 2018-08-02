try {
Write-Output "ProcessID  $PID"
while($true) {
    start-sleep -Seconds 2
    Write-Host "2 Seconds"
}
}
catch {
}
finally {
     Write-Output "script terminated"
}

