from setuptools import setup, Extension
from Cython.Build import cythonize

ext_modules = [
    Extension(
        "jessesort",  # The Python module name
        sources=["jessesort.pyx"],  # Cython source file
        language="c++",
        include_dirs=["src/jessesort/"],  # Ensure jesseSort.hpp is found
        libraries=["jesseSort"],  # Link against libjesseSort.so (no "lib" prefix)
        library_dirs=["src/jessesort/"],  # Directory containing libjesseSort.so
        extra_link_args=["-Wl,-rpath,src/jessesort/"],  # Fix runtime linking issues
    )
]

setup(
    name="jessesort",
    ext_modules=cythonize(
        ext_modules,
        compiler_directives={'language_level': 3}
    ),
)
