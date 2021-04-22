#!/usr/bin/env python3
import pathlib
import platform
import sys

from setuptools import Extension, setup
from setuptools.command.build_ext import build_ext

src_root = pathlib.Path(__file__).parent / "lib"
sources = [
    src_root.joinpath(s).as_posix() for s in ["Ppmd7.c", "Ppmd7Dec.c", "Ppmd7Enc.c", "Ppmd8.c", "Ppmd8Dec.c", "Ppmd8Enc.c"]
]

_ppmd_extension = Extension("pyppmd._ppmd", sources)

kwargs = {"include_dirs": ["lib"], "library_dirs": [], "libraries": [], "sources": sources, "define_macros": []}


def has_option(option):
    if option in sys.argv:
        sys.argv = [s for s in sys.argv if s != option]
        return True
    else:
        return False


if has_option("--cffi") or platform.python_implementation() == "PyPy":
    # packages
    packages = ["pyppmd", "pyppmd.cffi"]

    # binary extension
    kwargs["module_name"] = "pyppmd.cffi._cffi_ppmd"

    sys.path.append("src/ext")
    import ffi_build

    ffi_build.set_kwargs(**kwargs)
    binary_extension = ffi_build.ffibuilder.distutils_extension()
else:  # C implementation
    # packages
    packages = ["pyppmd", "pyppmd.c"]

    # binary extension
    kwargs["name"] = "pyppmd.c._ppmd"
    kwargs["sources"].append("src/ext/_ppmdmodule.c")

    binary_extension = Extension(**kwargs)


class build_ext_compiler_check(build_ext):
    def build_extensions(self):
        if "msvc" in self.compiler.compiler_type.lower():
            for extension in self.extensions:
                more_options = ["/Ob3", "/GF", "/Gy"]
                extension.extra_compile_args.extend(more_options)
        super().build_extensions()


setup(
    use_scm_version={"local_scheme": "no-local-version"},
    ext_modules=[binary_extension],
    package_dir={"": "src"},
    packages=packages,
    cmdclass={"build_ext": build_ext_compiler_check},
)
