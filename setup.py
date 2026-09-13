import sysconfig

from setuptools import Extension, setup

python_inc = sysconfig.get_path("include")
python_lib = sysconfig.get_config_var("LIBDIR")

setup(
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
            library_dirs=[python_lib],
            libraries=["sqlite3"],
            extra_compile_args=["-flto", "-std=c23"],
            extra_link_args=["-flto"],
        )
    ]
)
