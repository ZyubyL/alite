import sys
import sysconfig
from pathlib import Path

from setuptools import Extension, find_packages, setup

python_inc = sysconfig.get_path("include")
python_lib = sysconfig.get_config_var("LIBDIR")

vendor_dir = Path("src/alite/_core/vendor")

if sys.platform == "win32":
    extra_compile_args = ["/std:c17"]
    extra_link_args: list[str] = []
    include_dirs = [python_inc, str(vendor_dir)]
    sources = [
        "src/alite/_core/alite.c",
        "src/alite/_core/cursor.c",
        "src/alite/_core/connection.c",
        "src/alite/_core/pool.c",
    ]
    libraries: list[str] = []
    if (vendor_dir / "sqlite3.c").exists():
        sources.append(str(vendor_dir / "sqlite3.c"))
else:
    extra_compile_args = ["-flto", "-std=c17"]
    extra_link_args = ["-flto"]
    include_dirs = [python_inc]
    sources = [
        "src/alite/_core/alite.c",
        "src/alite/_core/cursor.c",
        "src/alite/_core/connection.c",
        "src/alite/_core/pool.c",
    ]
    libraries = ["sqlite3"]

setup(
    package_dir={"": "src"},
    packages=find_packages(where="src"),
    ext_modules=[
        Extension(
            "alite._alite",
            sources=sources,
            include_dirs=include_dirs,
            library_dirs=[python_lib] if python_lib else [],
            libraries=libraries,
            extra_compile_args=extra_compile_args,
            extra_link_args=extra_link_args,
        )
    ],
)
