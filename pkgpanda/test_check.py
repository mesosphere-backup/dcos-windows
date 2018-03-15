from subprocess import check_output, PIPE, Popen, STDOUT

from pkgpanda.util import is_windows, resources_test_dir

if is_windows:
    list_output = """pkg1--12345
 - hello_world_ok.py
 - not_executable.py
pkg2--12345
 - failed_check.py
 - shell_script_check.sh
"""
else:
    list_output = """WARNING: `not_executable.py` is not executable
pkg1--12345
 - hello_world_ok.py
pkg2--12345
 - failed_check.py
 - shell_script_check.sh
"""

run_output_stdout = """Hello World
I exist to fail...
Assertion error
Hello World
"""

run_output_stderr = """WARNING: `not_executable.py` is not executable
"""


def test_check_target_list():
    root_dir = resources_test_dir("opt/mesosphere")
    repo_dir = resources_test_dir("opt/mesosphere/packages")
    output = check_output([
        'pkgpanda',
        'check',
        '--list',
        '--root', root_dir,
        '--repository', repo_dir], stderr=STDOUT)
    output = output.decode().replace('\r', '')   # Eliminate the CRs that windows inserts
    assert output == list_output


def test_check_target_run():
    cmd = Popen([
        'pkgpanda',
        'check',
        '--root', resources_test_dir('opt/mesosphere'),
        '--repository', resources_test_dir('opt/mesosphere/packages')],
        stdout=PIPE, stderr=PIPE)
    stdout, stderr = cmd.communicate()
    stdout = stdout.decode().replace('\r', '')   # Eliminate the CRs that windows inserts
    stderr = stderr.decode().replace('\r', '')   # Eliminate the CRs that windows inserts
    assert stdout == run_output_stdout
    assert stderr == run_output_stderr
