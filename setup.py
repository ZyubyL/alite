import sys
import sysconfig

from setuptools import Extension, find_packages, setup

python_inc = sysconfig.get_path("include")
python_lib = sysconfig.get_config_var("LIBDIR")

if sys.platform == "win32":
    extra_compile_args = ["/std:c17"]
    extra_link_args: list[str] = []
else:
    extra_compile_args = ["-flto", "-std=c2x"]
    extra_link_args = ["-flto"]

setup(
    package_dir={"": "src"},
    packages=find_packages(where="src"),
    ext_modules=[
        Extension(
            "alite._alite",
            sources=[
                "src/alite/_core/alite.c",
                "src/alite/_core/cursor.c",
                "src/alite/_core/connection.c",
                "src/alite/_core/pool.c",
            ],
            include_dirs=[python_inc],
            library_dirs=[python_lib] if python_lib else [],
            libraries=["sqlite3"],
            extra_compile_args=extra_compile_args,
            extra_link_args=extra_link_args,
        )
    ],
)
