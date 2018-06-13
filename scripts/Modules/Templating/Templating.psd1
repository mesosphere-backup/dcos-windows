﻿# Copyright 2016 Cloudbase Solutions Srl
#
#    Licensed under the Apache License, Version 2.0 (the "License"); you may
#    not use this file except in compliance with the License. You may obtain
#    a copy of the License at
#
#         http://www.apache.org/licenses/LICENSE-2.0
#
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
#    WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
#    License for the specific language governing permissions and limitations
#    under the License.
#
# Module manifest for module 'Templating'
#
# Generated by: Gabriel Adrian Samfira
#
# Generated on: 19/04/2016
#

@{

# Script module or binary module file associated with this manifest.
RootModule = 'Templating.psm1'

# Version number of this module.
ModuleVersion = '0.1'

# ID used to uniquely identify this module
GUID = 'ba643132-bb99-4ef9-9aab-020300e8ed1e'

# Author of this module
Author = 'Gabriel Adrian Samfira'

# Company or vendor of this module
CompanyName = 'Cloudbase Solutions SRL'

# Copyright statement for this module
Copyright = '(c) 2016 Cloudbase Solutions SRL. All rights reserved.'

# Description of the functionality provided by this module
Description = 'Powershell module for rendering text templates'

# Minimum version of the Windows PowerShell engine required by this module
PowerShellVersion = '3.0'

# Script files (.ps1) that are run in the caller's environment prior to importing this module.
ScriptsToProcess = @("Load-Assemblies.ps1")

# Functions to export from this module
FunctionsToExport = (
    "Invoke-RenderTemplate",
    "Invoke-RenderTemplateFromFile"
    )

}
