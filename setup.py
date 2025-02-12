from setuptools import setup
from Cython.Build import cythonize

setup(
    ext_modules=cythonize("jessesort_c.pyx", language_level="3"),
)
