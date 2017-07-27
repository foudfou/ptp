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

from .mesonlib import Popen_safe

class StaticLinker:
    pass


class VisualStudioLinker(StaticLinker):
    always_args = ['/NOLOGO']

    def __init__(self, exelist):
        self.exelist = exelist

    def get_exelist(self):
        return self.exelist[:]

    def get_std_link_args(self):
        return []

    def get_buildtype_linker_args(self, buildtype):
        return []

    def get_output_args(self, target):
        return ['/OUT:' + target]

    def get_coverage_link_args(self):
        return []

    def get_always_args(self):
        return VisualStudioLinker.always_args

    def get_linker_always_args(self):
        return VisualStudioLinker.always_args

    def build_rpath_args(self, build_dir, from_dir, rpath_paths, build_rpath, install_rpath):
        return []

    def thread_link_flags(self):
        return []

    def get_option_link_args(self, options):
        return []

    @classmethod
    def unix_args_to_native(cls, args):
        from .compilers import VisualStudioCCompiler
        return VisualStudioCCompiler.unix_args_to_native(args)

    def get_link_debugfile_args(self, targetfile):
        # Static libraries do not have PDB files
        return []


class ArLinker(StaticLinker):

    def __init__(self, exelist):
        self.exelist = exelist
        self.id = 'ar'
        pc, stdo = Popen_safe(self.exelist + ['-h'])[0:2]
        # Enable deterministic builds if they are available.
        if '[D]' in stdo:
            self.std_args = ['csrD']
        else:
            self.std_args = ['csr']

    def build_rpath_args(self, build_dir, from_dir, rpath_paths, build_rpath, install_rpath):
        return []

    def get_exelist(self):
        return self.exelist[:]

    def get_std_link_args(self):
        return self.std_args

    def get_output_args(self, target):
        return [target]

    def get_buildtype_linker_args(self, buildtype):
        return []

    def get_linker_always_args(self):
        return []

    def get_coverage_link_args(self):
        return []

    def get_always_args(self):
        return []

    def thread_link_flags(self):
        return []

    def get_option_link_args(self, options):
        return []

    @classmethod
    def unix_args_to_native(cls, args):
        return args[:]

    def get_link_debugfile_args(self, targetfile):
        return []
