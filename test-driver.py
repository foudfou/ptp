#!/bin/env python
# Simple test driver
#
# Launches tests in parallel. Should be integrated to waf.
import multiprocessing
import subprocess
import shlex
import glob
import os

from multiprocessing.pool import ThreadPool

BUILD_DIR = 'build'

COLORS = {
    "red": '[0;31m',
    "grn": '[0;32m',
    "lgn": '[1;32m',
    "blu": '[1;34m',
    "mgn": '[0;35m',
    "std": '[m'
}


def call_proc(cmd):
    """ This runs in a separate thread. """
    p = subprocess.Popen(shlex.split(cmd),
                         stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    out, err = p.communicate()
    return (cmd, p.returncode, out, err)


if __name__ == '__main__':
    tests = []
    for f in glob.glob('tests/*.c'):
        path = os.path.join(BUILD_DIR, f[:-2])
        path = "/".join(path.split('\\'))  # win32
        tests.append(path)

    expected_failure = ""       # FIXME: each test can have an expected result
    pool = ThreadPool(multiprocessing.cpu_count())

    results = []
    for t in tests:
        results.append(pool.apply_async(call_proc, (t,)))

    # Close the pool and wait for each running task to complete
    pool.close()
    pool.join()
    for result in results:
        test_name, ret, out, err = result.get()

        chk = ":".join([str(ret), expected_failure])
        if chk.startswith('0:yes'):
            col=COLORS['red']; res='XPASS'; recheck='yes'; gcopy='yes'
        elif chk.startswith('0:'):
            col=COLORS['grn']; res='PASS'; recheck='no'; gcopy='no'
        elif chk.startswith('77:'):
            col=COLORS['blu']; res='SKIP'; recheck='no'; gcopy='yes'
        elif chk.startswith('99:'):
            col=COLORS['mgn']; res='ERROR'; recheck='yes'; gcopy='yes'
        elif chk.endswith(':yes'):
            col=COLORS['lgn']; res='XFAIL'; recheck='no'; gcopy='yes'
        else:
            col=COLORS['red']; res='FAIL'; recheck='yes'; gcopy='yes'

        # Report outcome to console.
        print("".join([col, res, COLORS['std'], '  ', test_name]))
        if ret:
            print("ret: {}\nout: {}\nerr: {}".format(ret, out, err))

    # subprocess.call("./merge_resized_images")
