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
        if expected in line.rstrip():
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

def check_file_not_exists(filename):
    import os

    global errors

    print("{BLUE}[RUN     ]{END} Checking {myfile} doesn't exist".format(myfile=filename, **formatters))
    if not os.path.isfile(filename):
        print("{BLUE}[      OK]{END} {myfile} doesn't exist".format(myfile=filename, **formatters))
    else:
        print("{RED}[   ERROR]{END} {myfile} found".format(myfile=filename, **formatters))
        errors += errors + 1


def run_io_test(test_env, file_uid, expect_logging):
    import subprocess
    import os

    global errors

    # Create output directory if specified
    if 'UMPIRE_OUTPUT_DIR' in test_env:
        output_dir = test_env['UMPIRE_OUTPUT_DIR']
        if not os.path.exists(output_dir):
            os.makedirs(output_dir)

    cmd_args = ['./io_tests']
    if expect_logging:
        cmd_args.append('--enable-logging')

    env = os.environ.copy()
    # Clear PMI_FD to avoid SLURM PMI interference when launching io_tests.
    env.pop('PMI_FD', None)

    test_program = subprocess.Popen(cmd_args,
            env=dict(env, **test_env),
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            shell=False)
    pid = test_program.pid
    ecode = test_program.wait()

    output = test_program.stdout
    error = test_program.stderr

    if ecode != 0:
        print("{RED}[   ERROR]{END} Unexpected exit code {ecode} from io_test".format(ecode=ecode, **formatters))
        for line in error:
            print(line)
        errors = 1
    else:
        # Default output directory is "./" when UMPIRE_OUTPUT_DIR is not set
        if 'UMPIRE_OUTPUT_DIR' in test_env.keys():
            output_filename = '{dir}/umpire_io_tests.{pid}.{uid}.log'.format(dir=test_env['UMPIRE_OUTPUT_DIR'], uid=file_uid, pid=pid)
        else:
            output_filename = './umpire_io_tests.{pid}.{uid}.log'.format(uid=file_uid, pid=pid)

        if expect_logging:
            check_file_exists(output_filename)
            with open(output_filename, 'rb') as output_file:
                # With spdlog, both Info and Error messages go to the log file
                check_output(output_filename, output_file, b'testing log stream')
            with open(output_filename, 'rb') as output_file:
                check_output(output_filename, output_file, b'testing error stream')
        else:
            check_file_not_exists(output_filename)

if __name__ == '__main__':
    import sys

    if sys.argv[1] == 'Basename +logging' and errors == 0:
        print("{BLUE}[--------]{END} io_test 1/3 Basename +logging".format(**formatters))
        run_io_test({'UMPIRE_OUTPUT_BASENAME' : 'umpire_io_tests'}, 0, True)

    if sys.argv[1] == 'Basename +Directory +logging' and errors == 0:
        print("{BLUE}[--------]{END} io_test 2/3 Basename +Directory +logging Test".format(**formatters))
        run_io_test({'UMPIRE_OUTPUT_DIR': './io_test_dir', 'UMPIRE_OUTPUT_BASENAME' : 'umpire_io_tests'}, 0, True)

    if sys.argv[1] == 'Basename +Directory -logging' and errors == 0:
        print("{BLUE}[--------]{END} io_test 3/3 Basename + Directory -logging Test".format(**formatters))
        run_io_test({'UMPIRE_OUTPUT_DIR': './optional_io_test_dir', 'UMPIRE_OUTPUT_BASENAME' : 'umpire_io_tests'}, 0, False)

    sys.exit(errors)
