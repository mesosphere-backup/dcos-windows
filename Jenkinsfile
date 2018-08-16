try {
    node('windows-build-server') {
        stage('Init') {
            setGitHubPullRequestStatus context: 'jenkins/hybrid-cloud-testing',
                                       message: 'Hybrid cloud testing started',
                                       state: 'PENDING'
            cleanWs()
            dir('mesos-jenkins') {
                git url: 'https://github.com/Microsoft/mesos-jenkins.git'
            }
        }

        stage('Generate blob') {
            powershell script: "mesos-jenkins/AgentBlob/GenerateAgentBlob.ps1 -GithubPRHeadSha ${GITHUB_PR_HEAD_SHA}"
        }

        stage('Publish blob') {
            withCredentials([file(credentialsId: 'ae8d8ce4-a601-4e1b-9b53-4bb117fc3451', variable: 'SSH_KEY')]) {
                powershell script: "mesos-jenkins/AgentBlob/PublishTestingAgentBlob.ps1 -ArtifactsDirectory \${env:WORKSPACE}\\artifacts -ReleaseVersion dcos-windows-pr-${GITHUB_PR_NUMBER}-${GITHUB_PR_HEAD_SHA}"
            }
        }
    }

    node('dummy-slave') {
        stage('Hybrid cloud testing') {
            cleanWs()
            dir('mesos-jenkins') {
                git url: 'https://github.com/Microsoft/mesos-jenkins.git'
            }
            withCredentials([usernamePassword(credentialsId: '8d70c82f-6959-49ad-a553-e7906ad47710',
                                              passwordVariable: 'JENKINS_PASSWORD', 
                                              usernameVariable: 'JENKINS_USER'),
                             usernamePassword(credentialsId: 'e20c9faa-1d51-46fa-8e69-a027c7f3f8c8',
                                              passwordVariable: 'AZURE_SERVICE_PRINCIPAL_PASSWORD',
                                              usernameVariable: 'AZURE_SERVICE_PRINCIPAL_ID'),
                             usernamePassword(credentialsId: '49dc70d1-13d5-4b52-8944-d62f5a2474e3',
                                              passwordVariable: 'DOCKER_HUB_USER_PASSWORD',
                                              usernameVariable: 'DOCKER_HUB_USER')]) {
                withEnv(["DCOS_DEPLOYMENT_TYPE=hybrid",
                         "DCOS_WINDOWS_BOOTSTRAP_URL=http://dcos-win.westus.cloudapp.azure.com/dcos-windows/testing/windows-agent-blob/dcos-windows-pr-${GITHUB_PR_NUMBER}-${GITHUB_PR_HEAD_SHA}",
                         "MASTER_WHITELISTED_IPS=13.66.169.239 13.66.171.181 13.66.173.101 13.66.215.214 13.77.172.69",
                         "AUTOCLEAN=true",
                         "SET_CLEANUP_TAG=true",
                         "AZURE_KEYVAULT_NAME=ci-key-vault",
                         "PRIVATE_KEY_SECRET_NAME=jenkins-dcos-testing-ssh-private-key",
                         "PUBLIC_KEY_SECRET_NAME=jenkins-dcos-testing-ssh-public-key",
                         "WIN_PASS_SECRET_NAME=jenkins-dcos-testing-win-pass",
                         "AZURE_REGION=eastus",
                         "WIN_AGENT_SIZE=Standard_D2s_v3",
                         "LINUX_MASTER_SIZE=Standard_D2s_v3",
                         "LINUX_AGENT_SIZE=Standard_D2s_v3",
                         "AZURE_SERVICE_PRINCIPAL_TENAT=72f988bf-86f1-41af-91ab-2d7cd011db47",
                         "DCOS_DEPLOY_DIR=$WORKSPACE/dcos-deploy-dir",
                         "JOB_ARTIFACTS_DIR=$HOME/artifacts/$JOB_NAME",
                         "AZURE_CONFIG_DIR=$WORKSPACE/azure_$BUILD_ID"]) {
                    sh script: "mesos-jenkins/DCOS/start-dcos-e2e-testing.sh"
                }
            }
        }
        setGitHubPullRequestStatus context: "jenkins/hybrid-cloud-testing",
                                   message: "Hybrid cloud testing result: SUCCESS",
                                   state: 'SUCCESS'
    }
} catch (e) {
    node('dummy-slave') {
        setGitHubPullRequestStatus context: "jenkins/hybrid-cloud-testing",
                                   message: "Hybrid cloud testing result: FAILURE",
                                   state: 'FAILURE'
   }
   throw e
}
