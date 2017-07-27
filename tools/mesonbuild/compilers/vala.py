# Copyright 2012-2017 The Meson development team

# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at

#     http://www.apache.org/licenses/LICENSE-2.0

# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import os.path

from .. import mlog
from ..mesonlib import EnvironmentException

from .compilers import Compiler

class ValaCompiler(Compiler):
    def __init__(self, exelist, version):
        self.language = 'vala'
        super().__init__(exelist, version)
        self.version = version
        self.id = 'valac'
        self.is_cross = False

    def name_string(self):
        return ' '.join(self.exelist)

    def needs_static_linker(self):
        return False # Because compiles into C.

    def get_output_args(self, target):
        return ['-o', target]

    def get_compile_only_args(self):
        return ['-C']

    def get_pic_args(self):
        return []

    def get_always_args(self):
        return ['-C']

    def get_warn_args(self, warning_level):
        return []

    def get_no_warn_args(self):
        return ['--disable-warnings']

    def get_werror_args(self):
        return ['--fatal-warnings']

    def sanity_check(self, work_dir, environment):
        code = 'class MesonSanityCheck : Object { }'
        args = self.get_cross_extra_flags(environment, link=False)
        with self.compile(code, args, 'compile') as p:
            if p.returncode != 0:
                msg = 'Vala compiler {!r} can not compile programs' \
                      ''.format(self.name_string())
                raise EnvironmentException(msg)

    def get_buildtype_args(self, buildtype):
        if buildtype == 'debug' or buildtype == 'debugoptimized' or buildtype == 'minsize':
            return ['--debug']
        return []

    def find_library(self, libname, env, extra_dirs):
        if extra_dirs and isinstance(extra_dirs, str):
            extra_dirs = [extra_dirs]
        # Valac always looks in the default vapi dir, so only search there if
        # no extra dirs are specified.
        if not extra_dirs:
            code = 'class MesonFindLibrary : Object { }'
            vapi_args = ['--pkg', libname]
            args = self.get_cross_extra_flags(env, link=False)
            args += vapi_args
            with self.compile(code, args, 'compile') as p:
                if p.returncode == 0:
                    return vapi_args
        # Not found? Try to find the vapi file itself.
        for d in extra_dirs:
            vapi = os.path.join(d, libname + '.vapi')
            if os.path.isfile(vapi):
                return [vapi]
        mlog.debug('Searched {!r} and {!r} wasn\'t found'.format(extra_dirs, libname))
        return None
