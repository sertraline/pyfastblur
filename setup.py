from setuptools.command.build_py import build_py as _build_py
from distutils.core import setup, Extension
import os
from glob import glob
from shutil import move

this_directory = os.path.abspath(os.path.dirname(__file__))
with open(os.path.join(this_directory, 'README.md'), encoding='utf-8') as f:
    long_description = f.read()

class build_py(_build_py):
    def run(self):
        self.run_command("build_ext")
        if os.name == 'nt':
            pyd = glob('pyfastblur/*.pyd')
        else:
            pyd = glob('pyfastblur/*.so')
        build_dir = os.path.join('pyfastblur', 'libpyfastblur')
        if not os.path.isdir(build_dir):
            os.mkdir(build_dir)

        if os.name == 'nt':
            build_file = os.path.join(build_dir, 'pyfastblur_cpp.pyd')
        else:
            build_file = os.path.join(build_dir, 'pyfastblur_cpp.so')
        if not os.path.isfile(build_file):
            move(pyd[0], build_file)
        else:
            os.replace(pyd[0], build_file)
        with open(os.path.join(build_dir, '__init__.py'), 'w') as file:
            file.write('from .pyfastblur_cpp import *')
        return super().run()


VERSION = [('MAJOR_VERSION', '1'),
           ('MINOR_VERSION', '0')]

if os.name == 'nt':
    bmodule = Extension('pyfastblur_cpp',
                        define_macros=VERSION,
                        include_dirs=['pyfastblur/src/win'],
                        library_dirs=['pyfastblur/src/win'],
                        libraries=['libpng16', 'zlib'],
                        sources=['pyfastblur/src/blur.cpp']
                        )
    packages = {"pyfastblur/libpyfastblur": ['__init__.py', 'pyfastblur_cpp.pyd', 'libpng16.dll', 'zlib.dll']}

else:
    bmodule = Extension('pyfastblur_cpp',
                        define_macros=VERSION,
                        include_dirs=['pyfastblur/src/unix', '/usr/local/include'],
                        library_dirs=['pyfastblur/src/unix', '/usr/local/lib'],
                        libraries=['png16', 'z'],
                        sources=['pyfastblur/src/blur.cpp']
                        )
    packages = {"pyfastblur/libpyfastblur": ['__init__.py', 'pyfastblur_cpp.so', 'png16.so', 'z.so']}

setup(name='pyfastblur',
      version='1.0',
      description='Small Python library with a single purpose to apply fast blur to PNG images (libpng backend)',
      author='Toshiro Iwa',
      author_email='iwa@acid.im',
      long_description=long_description,
      long_description_content_type='text/markdown',
      url='https://github.com/sertraline/pyfastblur',
      packages=["pyfastblur", "pyfastblur/libpyfastblur"],
      package_data=packages,
      include_package_data=True,
      ext_modules=[bmodule],
      cmdclass={'build_py': build_py},
      )