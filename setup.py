#!/usr/bin/env python3
import os
import platform
import sys

from setuptools import Extension, setup
from setuptools.command.build_ext import build_ext
from setuptools.command.egg_info import egg_info

sources = [
    "lib/Ppmd7.c",
    "lib/Ppmd8.c",
    "lib/Ppmd8Dec.c",
    "lib/Ppmd7Enc.c",
    "lib/Ppmd8Enc.c",
    "lib/Ppmd7Dec.c",
    "lib2/Buffer.c",
    "lib2/ThreadDecoder.c",
]
_ppmd_extension = Extension("pyppmd._ppmd", sources)
kwargs = {"include_dirs": ["lib", "lib2"], "library_dirs": [], "libraries": [], "sources": sources, "define_macros": []}


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


WARNING_AS_ERROR = has_option("--warning-as-error")


class build_ext_compiler_check(build_ext):
    def build_extensions(self):
        for extension in self.extensions:
            if self.compiler.compiler_type.lower() in ("unix", "mingw32"):
                if WARNING_AS_ERROR:
                    extension.extra_compile_args.append("-Werror")
            elif self.compiler.compiler_type.lower() == "msvc":
                # /GF eliminates duplicate strings
                # /Gy does function level linking
                more_options = ["/GF", "/Gy"]
                if WARNING_AS_ERROR:
                    more_options.append("/WX")
                extension.extra_compile_args.extend(more_options)
        super().build_extensions()


# Work around pypa/setuptools#436.
class my_egg_info(egg_info):
    def run(self):
        try:
            os.remove(os.path.join(self.egg_info, "SOURCES.txt"))
        except FileNotFoundError:
            pass
        super().run()


setup(
    use_scm_version={"local_scheme": "no-local-version"},
    ext_modules=[binary_extension],
    package_dir={"": "src"},
    packages=packages,
    cmdclass={"build_ext": build_ext_compiler_check, "egg_info": my_egg_info},
)
