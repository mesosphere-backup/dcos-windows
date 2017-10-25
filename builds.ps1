
#
#
$global:sources_done = @()

function Do-Source {
    param (
            [string] $source
            [string] $destpath
          )
    Write-Host "build source "
    

function Do-Build {
    param ( [string] $buildpath, 
                [string] $destpath
              )

    $obj = Get-Content "$buildpath/buildinfo.json"
    if ($obj.sources.count -eq 1) {
        $global:source_done += Do-Source $obj.sources $destpath
    }
    else {
        foreach ($source in $obj.sources) {
           $global:sources_done += Do-Source $source $destpath
        }
    }
}    


  
