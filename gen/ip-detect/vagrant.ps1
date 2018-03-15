( get-netipaddress  | where { ( $_.AddressFamily.toString() -eq "IPv4" ) -and ( $_.InterfaceAlias -like "*vEthernet*") -and !($_.InterfaceAlias -like "*nat*")} ).IPAddress.toString()
