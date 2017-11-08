# dcos-windows
Build project for dc/os on windows. 

*Note: This is a work in progress and does not work. Do Not Use. *

## Structure.

### packages tree

The packages tree is zipped into the deploy file packages.zip. In this for it is sent to the 

The packages directory separates the build and setup process. Each directory contains:
  - A json file (setupinfo.json) describing the name of the package, any dependencies, and the name of the script which will perform the setup operation.
  - A setup script (setup-xxx.ps1) which implements a powershell "Installable" class. The methods on that class are Setup() and Update(). 
  - A buildinfo.json file whic parallels the buildinfo.json found in the dcos/dcos pacakges directories. This contains information regarding building or acquiring the package. 
  - A build script (build-xxx.ps1) which performs the build operation.
  - Extra files, such as xml, templates etc required to perform the setup or build operations.

There is a separate script (packages.ps1) which walks the packages tree.  For each directory in the tree, it will perform the setup or build for each dependent package, then the package setup or build of that directory. packages.ps1 keeps a dictionary of package setup or build results, and for each directory it visits, will check first to see if the operation has been completed. As a result, each package directory will be visited exactly once.

The operation of packages.ps1 parallels that of pkgpanda in the dcos/dcos repo.

The scripts for build or setup operations in each package are expected to stand alone, without reference to other code beyond stated dependencies.




