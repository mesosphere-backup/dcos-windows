Set-PSDebug -off
try {

    while($true) {
        start-sleep -Seconds 2
        Write-Host "2 Seconds"
    }
}
catch {
}
finally {
     Write-Host "script terminated"
}

