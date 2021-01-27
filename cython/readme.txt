Here we try to use Cython to provide a Python interface.

To install,
    python setup.py build_ext --inplace
Test run,
    LD_LIBRARY_PATH=../lib:$LD_LIBRARY_PATH python3 test_scm.py
