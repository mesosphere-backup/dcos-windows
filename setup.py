from setuptools import setup
import platform
is_windows = (platform.system() == "Windows")

def get_advanced_templates():
    template_base = 'aws/templates/advanced/'
    template_names = ['advanced-master', 'advanced-priv-agent', 'advanced-pub-agent', 'infra', 'zen']

    return [template_base + name + '.json' for name in template_names]

if is_windows:
    packages=[
        'gen',
        'gen.build_deploy',
        'pkgpanda',
        'pkgpanda.build',
        'pkgpanda.http',
        'release',
        'release.storage',
        ]

    package_data={
        'gen': [
            'ip-detect/aws.ps1',
            'ip-detect/aws_public.ps1',
            'ip-detect/azure.ps1',
            'ip-detect/vagrant.ps1',
            'fault-domain-detect/aws.ps1',
            'fault-domain-detect/azure.ps1',
            'cloud-config.yaml',
            'dcos-windows-config.yaml',
            'dcos-metadata.yaml',
            'dcos-windows-services.yaml',
            'aws/dcos-config.yaml',
            'aws/templates/aws.html',
            'aws/templates/cloudformation.json',
            'azure/cloud-config.yaml',
            'azure/azuredeploy-parameters.json',
            'azure/templates/acs.json',
            'azure/templates/azure.html',
            'azure/templates/azuredeploy.json',
            'build_deploy/pwsh/dcos_generate_config.ps1.in',
            'build_deploy/pwsh/installer_internal_wrapper.in',
            'build_deploy/pwsh/dcos-launch.spec',
            'coreos-aws/cloud-config.yaml',
            'coreos/cloud-config.yaml'
        ] + get_advanced_templates(),
        'pkgpanda': [
            'docker/dcos-builder-windows/Dockerfile'
        ]
    }
    zip_safe=True
else:
    packages=[
        'dcos_installer',
        'gen',
        'gen.build_deploy',
        'pkgpanda',
        'pkgpanda.build',
        'pkgpanda.http',
        'release',
        'release.storage',
        'ssh']
    package_data={
        'gen': [
            'ip-detect/aws.sh',
            'ip-detect/aws_public.sh',
            'ip-detect/azure.sh',
            'ip-detect/vagrant.sh',
            'fault-domain-detect/aws.sh',
            'fault-domain-detect/azure.sh',
            'cloud-config.yaml',
            'dcos-config.yaml',
            'dcos-metadata.yaml',
            'dcos-services.yaml',
            'aws/dcos-config.yaml',
            'aws/templates/aws.html',
            'aws/templates/cloudformation.json',
            'azure/cloud-config.yaml',
            'azure/azuredeploy-parameters.json',
            'azure/templates/acs.json',
            'azure/templates/azure.html',
            'azure/templates/azuredeploy.json',
            'build_deploy/bash/dcos_generate_config.sh.in',
            'build_deploy/bash/Dockerfile.in',
            'build_deploy/bash/installer_internal_wrapper.in',
            'build_deploy/bash/dcos-launch.spec',
            'coreos-aws/cloud-config.yaml',
            'coreos/cloud-config.yaml'
        ] + get_advanced_templates(),
        'pkgpanda': [
            'docker/dcos-builder/Dockerfile'
        ]
    }
    zip_safe=False

setup(
    name='dcos_image',
    version='0.1',
    description='DC/OS cluster configuration, assembly, and maintenance code',
    url='https://dcos.io',
    author='Mesosphere, Inc.',
    author_email='help@dcos.io',
    license='apache2',
    classifiers=[
        'Development Status :: 3 - Alpha',
        'License :: OSI Approved :: Apache Software License',
        'Programming Language :: Python :: 3',
        'Programming Language :: Python :: 3.6',
    ],
    packages=packages,
    install_requires=[
        'aiohttp==0.22.5',
        'analytics-python',
        'coloredlogs',
        'Flask',
        'flask-compress',
        # Pins taken from 'azure==2.0.0rc4'
        'msrest==0.4.0',
        'msrestazure==0.4.1',
        'azure-common==1.1.4',
        'azure-storage==0.32.0',
        'azure-mgmt-network==0.30.0rc4',
        'azure-mgmt-resource==0.30.0rc4',
        'boto3',
        'botocore',
        'checksumdir',
        'coloredlogs',
        'docopt',
        'passlib',
        'py',
        'pytest',
        'pyyaml',
        'requests==2.10.0',
        'retrying',
        'schema',
        'keyring==9.1',  # FIXME: pin keyring to prevent dbus dep
        'teamcity-messages'],
    entry_points={
        'console_scripts': [
            'release=release:main',
            'pkgpanda=pkgpanda.cli:main',
            'mkpanda=pkgpanda.build.cli:main',
            'dcos_installer=dcos_installer.cli:main',
        ],
    },
    package_data=package_data,
    zip_safe=zip_safe
)
