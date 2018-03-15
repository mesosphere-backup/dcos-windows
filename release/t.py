import sys
import os
import json
import yaml

sys.path.append(os.path.abspath("."))
import gen
import logging as log

from pathlib import Path

from gen.calc import entry

from pkgpanda import is_windows


args2 = {
    "provider": "azure",
    "platform": "azure",
    "resolvers": '["168.63.129.16"]',
    "master_discovery" : "static",
    "exhibitor_storage_backend": "azure",
    "enable_docker_gc": "false",
    "exhibitor_azure_prefix": "%uniqueName",
    "ip_detect_filename": "c:/tmp/azure-ip-detect.ps1",
    "master_list":'[ "192.168.255.5", "192.168.255.6", "192.168.255.7" ]',
    "package_ids": '["package_ids--placeholder"]',
    "bootstrap_url": "https://byteme.com",
    "exhibitor_azure_account_name": "%azureuser",
    "exhibitor_azure_account_key": "%exhibitor_azure_account_key",
    "cluster_name": "%cluster_name",
    "bootstrap_id": "%bootstrap_id"
}

    #
    # Resolve the yaml files (this should be a function)
    #

def generate_yamls(arguments:dict,
                   extra_templates=list(),
                   extra_sources=list(),
                   extra_targets=list()):

    user_arguments = arguments

    sources,targets,templates = gen.get_dcosconfig_source_target_and_templates(
            user_arguments, extra_templates, extra_sources)

    resolver = gen.validate_and_raise(sources, targets)
    argument_dict = gen.get_final_arguments(resolver)

    late_variables = gen.get_late_variables(resolver, sources)
    secret_builtins = ['expanded_config_full', 'user_arguments_full', 'config_yaml_full']
    secret_variables = set(gen.get_secret_variables(sources) + secret_builtins)
    masked_value = '**HIDDEN**'

    # Calculate values for builtin variables.
    user_arguments_masked = {k: (masked_value if k in secret_variables else v) for k, v in user_arguments.items()}
    argument_dict['user_arguments_full'] = gen.json_prettyprint(user_arguments)
    argument_dict['user_arguments']     = gen.json_prettyprint(user_arguments_masked)
    argument_dict['config_yaml_full']   = gen.user_arguments_to_yaml(user_arguments)
    argument_dict['config_yaml'] = gen.user_arguments_to_yaml(user_arguments_masked)

    # The expanded_config and expanded_config_full variables contain all other variables and their values.
    # expanded_config is a copy of expanded_config_full with secret values removed. Calculating these variables' values
    # must come after the calculation of all other variables to prevent infinite recursion.
    # TODO(cmaloney): Make this late-bound by gen.internals
    expanded_config_full = {
        k: v for k, v in argument_dict.items()
        # Omit late-bound variables whose values have not yet been calculated.
        if not v.startswith(gen.internals.LATE_BIND_PLACEHOLDER_START)
    }
    expanded_config_scrubbed = {k: v for k, v in expanded_config_full.items() if k not in secret_variables}
    argument_dict['expanded_config_full'] = gen.format_expanded_config(expanded_config_full)
    argument_dict['expanded_config'] = gen.format_expanded_config(expanded_config_scrubbed)

    log.debug(
        "Final arguments:" + gen.json_prettyprint({
            # Mask secret config values.
            k: (masked_value if k in secret_variables else v) for k, v in argument_dict.items()
        })
    )

    rendered_tmpls = gen.render_templates(templates, argument_dict)

    for tmpl in rendered_tmpls.keys(): 
        print(tmpl)
        with open(tmpl+".rendered", "w") as outfile:
            yaml.dump(rendered_tmpls[tmpl], outfile, indent=4,  default_style='|', default_flow_style=False)




if __name__ == '__main__':

    # resolve the configuration files
    generate_yamls(args2)

    # Given the resolved YAML files, extract the contents

