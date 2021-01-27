from setuptools import Extension, setup
from Cython.Build import cythonize

ext_modules = [Extension("scm", sources=["scm.pyx", "call_py_obj.pyx"], libraries=["scm"], include_dirs=[".."], library_dirs=["../lib"]), ]

setup(
    name='scm',
    ext_modules=cythonize(ext_modules, annotate=True),
    zip_safe=False,
    )
