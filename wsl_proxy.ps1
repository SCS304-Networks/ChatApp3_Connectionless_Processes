# refresh-wsl-proxy.ps1
$wslIp = (wsl -- hostname -I).Trim()

if (-not $wslIp) {
    Write-Host "ERROR: Could not get WSL2 IP. Make sure WSL2 is running."
    exit 1
}

netsh interface portproxy delete v4tov4 listenaddress=0.0.0.0 listenport=8080
netsh interface portproxy delete v4tov4 listenaddress=0.0.0.0 listenport=8081

netsh interface portproxy add v4tov4 listenaddress=0.0.0.0 listenport=8080 connectaddress=$wslIp connectport=8080
netsh interface portproxy add v4tov4 listenaddress=0.0.0.0 listenport=8081 connectaddress=$wslIp connectport=8081

Write-Host "WSL2 proxy updated to $wslIp"