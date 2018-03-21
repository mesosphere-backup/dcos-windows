# A recommended procedure for setting up a DCOS cluster using asc-engine for developement

## From a Linux VM:

You will need to:

1.	Download and install the azure cli 1.2
2.	Download and install go 
3.  Get the acs-engine source code.
4.	Create an acs-engine template for the cluster
5.	Login to azure
6.	Create a resource group
7.	Create the deployment.
8.	Attach to the DCOS UI
9.	Login to a windows public agent
10.	Run a service.

So:
### 1.	Download and install the azure cli 1.2

On Ubuntu:  (these instructions are from 
        echo "deb [arch=amd64] https://packages.microsoft.com/repos/azure-cli/ wheezy main" | \
     sudo tee /etc/apt/sources.list.d/azure-cli.list
     
     sudo apt-key adv --keyserver packages.microsoft.com --recv-keys 417A0893
     sudo apt-get install apt-transport-https
     sudo apt-get update && sudo apt-get install azure-cli 
### 2. Download and install go

Download and untar the latest go from https://golang.org/dl/ . Do *not* get the deb package as it is very back level and will not work.
We recommend installing it in /opt/go. Add the bin directory to your path. In addition set the GOPATH and GOROOT env variables. 
```
  export GOPATH=$HOME/go
  export GOROOT=/opt/go
```
3.	Create an acs-engine template for the cluster
There are examples in acs-engine/exammples.
