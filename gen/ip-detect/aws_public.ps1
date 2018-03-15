#!/usr/bin/pwsh
//set -o nounset -o errexit

func get_private_ip_from_metaserver()
{
    // fail silently but show an error message
    // -L follow a location    
    ((Invoke-WebRequest -URI http://169.254.169.254/latest/meta-data/local-ipv4 ).Content
$headers = @{"Metadata" = "true"}
$r = (Invoke-WebRequest -headers $headers "http://169.254.169.254/latest/meta-data/local-ipv4" -UseBasicParsing ).Conttent
}


$addr=$(get_private_ip_from_metaserver)
if ($addr -ne "") {
    $env:COREOS_PRIVATE_IPV4=$(get_private_ip_from_metaserver)}
}
$env:COREOS_PRIVATE_IPV4
