##############################################################################
# Copyright (c) 2016-26, Lawrence Livermore National Security, LLC and Umpire
# project contributors. See the COPYRIGHT file for details.
#
# SPDX-License-Identifier: (MIT)
##############################################################################

formatters = {
    'RED': '\033[91m',
    'BLUE': '\033[94m',
    'END': '\033[0m',
}


errors = 0

def check_output(name, file_object, expected):
    global errors

    print("{BLUE}[RUN     ]{END} Checking for \"{expected}\" in {name}".format(name=name, expected=expected, **formatters))

    found = False

    for line in file_object:
        if expected in line:
            found = True

    if not found:
        print("{RED}[   ERROR]{END} Got {contents}".format(contents=file_object.read(), expected=expected, **formatters))
        errors = errors + 1
    else:
        print("{BLUE}[      OK]{END} Found \"{expected}\" in {name}".format(name=name, expected=expected, **formatters))


def check_file_exists(filename):
    import os

    global errors

    print("{BLUE}[RUN     ]{END} Checking {myfile} exists".format(myfile=filename, **formatters))
    if not os.path.isfile(filename):
        print("{RED}[   ERROR]{END} {myfile} not found".format(myfile=filename, **formatters))
        errors += errors + 1
    else:
        print("{BLUE}[      OK]{END} {myfile} exists".format(myfile=filename, **formatters))


def run_test():
    import subprocess
    import os

    # Test 1: Basic logging with Debug level
    print("\n{BLUE}Test 1: Basic logging with Debug level{END}".format(**formatters))
    test_env = {"UMPIRE_LOG_LEVEL" : "Debug"}
    cmd_args = ['./log_tests']
    test_program = subprocess.Popen(cmd_args,
            env=dict(os.environ, **test_env),
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            shell=False)
    pid = test_program.pid
    test_program.wait()

    file_uid = 0;

    output = test_program.stdout

    output_filename = 'umpire.{pid}.{uid}.log'.format(uid=file_uid, pid=pid)

    check_file_exists(output_filename)
    with open(output_filename) as output_file:
        check_output(output_filename, output_file, 'Test log message from log_tests')

    # Test 2: Async logging
    print("\n{BLUE}Test 2: Async logging{END}".format(**formatters))
    test_env = {
        "UMPIRE_LOG_LEVEL": "Info",
        "UMPIRE_LOG_ASYNC": "on",
        "UMPIRE_LOG_QUEUE_SIZE": "4096"
    }
    test_program = subprocess.Popen(cmd_args,
            env=dict(os.environ, **test_env),
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            shell=False)
    pid = test_program.pid
    test_program.wait()

    output_filename = 'umpire.{pid}.{uid}.log'.format(uid=file_uid, pid=pid)
    check_file_exists(output_filename)
    with open(output_filename) as output_file:
        check_output(output_filename, output_file, 'Test log message from log_tests')

    # Test 3: Console disabled by default (file-only logging)
    print("\n{BLUE}Test 3: Console output disabled by default{END}".format(**formatters))
    test_env = {
        "UMPIRE_LOG_LEVEL": "Info"
    }
    test_program = subprocess.Popen(cmd_args,
            env=dict(os.environ, **test_env),
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            shell=False)
    pid = test_program.pid
    test_program.wait()

    # stderr should be empty when console is disabled (the default)
    global errors
    stderr_output = test_program.stderr.read().decode('utf-8')
    if 'Test log message from log_tests' in stderr_output:
        print("{RED}[   ERROR]{END} Found log message in stderr when console should be disabled".format(**formatters))
        errors += 1
    else:
        print("{BLUE}[      OK]{END} Console output correctly disabled by default".format(**formatters))

    # File should still have the log
    output_filename = 'umpire.{pid}.{uid}.log'.format(uid=file_uid, pid=pid)
    check_file_exists(output_filename)
    with open(output_filename) as output_file:
        check_output(output_filename, output_file, 'Test log message from log_tests')

    # Test 4: Console enabled explicitly
    print("\n{BLUE}Test 4: Console output enabled{END}".format(**formatters))
    test_env = {
        "UMPIRE_LOG_LEVEL": "Info",
        "UMPIRE_LOG_TO_CONSOLE": "on"
    }
    test_program = subprocess.Popen(cmd_args,
            env=dict(os.environ, **test_env),
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            shell=False)
    pid = test_program.pid
    test_program.wait()

    # stderr should contain the log message when console is enabled
    stderr_output = test_program.stderr.read().decode('utf-8')
    if 'Test log message from log_tests' in stderr_output:
        print("{BLUE}[      OK]{END} Console output correctly enabled".format(**formatters))
    else:
        print("{RED}[   ERROR]{END} Log message missing from stderr when console is enabled".format(**formatters))
        errors += 1

    # File should have the log too
    output_filename = 'umpire.{pid}.{uid}.log'.format(uid=file_uid, pid=pid)
    check_file_exists(output_filename)
    with open(output_filename) as output_file:
        check_output(output_filename, output_file, 'Test log message from log_tests')

if __name__ == '__main__':
    import sys

    print("{BLUE}[--------]{END}".format(**formatters))
    run_test()
    print("{BLUE}[--------]{END}".format(**formatters))
    sys.exit(errors)
