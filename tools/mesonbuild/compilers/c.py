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

import subprocess, os.path, tempfile

from .. import mlog
from .. import coredata
from ..mesonlib import EnvironmentException, version_compare, Popen_safe

from .compilers import (
    GCC_MINGW,
    get_largefile_args,
    gnu_winlibs,
    msvc_buildtype_args,
    msvc_buildtype_linker_args,
    msvc_winlibs,
    vs32_instruction_set_args,
    vs64_instruction_set_args,
    ClangCompiler,
    Compiler,
    CompilerArgs,
    CrossNoRunException,
    GnuCompiler,
    IntelCompiler,
    RunResult,
)


class CCompiler(Compiler):
    def __init__(self, exelist, version, is_cross, exe_wrapper=None):
        # If a child ObjC or CPP class has already set it, don't set it ourselves
        if not hasattr(self, 'language'):
            self.language = 'c'
        super().__init__(exelist, version)
        self.id = 'unknown'
        self.is_cross = is_cross
        self.can_compile_suffixes.add('h')
        if isinstance(exe_wrapper, str):
            self.exe_wrapper = [exe_wrapper]
        else:
            self.exe_wrapper = exe_wrapper

    def needs_static_linker(self):
        return True # When compiling static libraries, so yes.

    def get_always_args(self):
        '''
        Args that are always-on for all C compilers other than MSVC
        '''
        return ['-pipe'] + get_largefile_args(self)

    def get_linker_debug_crt_args(self):
        """
        Arguments needed to select a debug crt for the linker
        This is only needed for MSVC
        """
        return []

    def get_no_stdinc_args(self):
        return ['-nostdinc']

    def get_no_stdlib_link_args(self):
        return ['-nostdlib']

    def get_warn_args(self, level):
        return self.warn_args[level]

    def get_no_warn_args(self):
        # Almost every compiler uses this for disabling warnings
        return ['-w']

    def get_soname_args(self, prefix, shlib_name, suffix, path, soversion, is_shared_module):
        return []

    def split_shlib_to_parts(self, fname):
        return None, fname

    # The default behavior is this, override in MSVC
    def build_rpath_args(self, build_dir, from_dir, rpath_paths, build_rpath, install_rpath):
        return self.build_unix_rpath_args(build_dir, from_dir, rpath_paths, build_rpath, install_rpath)

    def get_dependency_gen_args(self, outtarget, outfile):
        return ['-MMD', '-MQ', outtarget, '-MF', outfile]

    def depfile_for_object(self, objfile):
        return objfile + '.' + self.get_depfile_suffix()

    def get_depfile_suffix(self):
        return 'd'

    def get_exelist(self):
        return self.exelist[:]

    def get_linker_exelist(self):
        return self.exelist[:]

    def get_preprocess_only_args(self):
        return ['-E', '-P']

    def get_compile_only_args(self):
        return ['-c']

    def get_no_optimization_args(self):
        return ['-O0']

    def get_compiler_check_args(self):
        '''
        Get arguments useful for compiler checks such as being permissive in
        the code quality and not doing any optimization.
        '''
        return self.get_no_optimization_args()

    def get_output_args(self, target):
        return ['-o', target]

    def get_linker_output_args(self, outputname):
        return ['-o', outputname]

    def get_coverage_args(self):
        return ['--coverage']

    def get_coverage_link_args(self):
        return ['--coverage']

    def get_werror_args(self):
        return ['-Werror']

    def get_std_exe_link_args(self):
        return []

    def get_include_args(self, path, is_system):
        if path == '':
            path = '.'
        if is_system:
            return ['-isystem', path]
        return ['-I' + path]

    def get_std_shared_lib_link_args(self):
        return ['-shared']

    def get_library_dirs(self):
        stdo = Popen_safe(self.exelist + ['--print-search-dirs'])[1]
        for line in stdo.split('\n'):
            if line.startswith('libraries:'):
                libstr = line.split('=', 1)[1]
                return libstr.split(':')
        return []

    def get_pic_args(self):
        return ['-fPIC']

    def name_string(self):
        return ' '.join(self.exelist)

    def get_pch_use_args(self, pch_dir, header):
        return ['-include', os.path.split(header)[-1]]

    def get_pch_name(self, header_name):
        return os.path.split(header_name)[-1] + '.' + self.get_pch_suffix()

    def get_linker_search_args(self, dirname):
        return ['-L' + dirname]

    def gen_import_library_args(self, implibname):
        """
        The name of the outputted import library

        This implementation is used only on Windows by compilers that use GNU ld
        """
        return ['-Wl,--out-implib=' + implibname]

    def sanity_check_impl(self, work_dir, environment, sname, code):
        mlog.debug('Sanity testing ' + self.get_display_language() + ' compiler:', ' '.join(self.exelist))
        mlog.debug('Is cross compiler: %s.' % str(self.is_cross))

        extra_flags = []
        source_name = os.path.join(work_dir, sname)
        binname = sname.rsplit('.', 1)[0]
        if self.is_cross:
            binname += '_cross'
            if self.exe_wrapper is None:
                # Linking cross built apps is painful. You can't really
                # tell if you should use -nostdlib or not and for example
                # on OSX the compiler binary is the same but you need
                # a ton of compiler flags to differentiate between
                # arm and x86_64. So just compile.
                extra_flags += self.get_cross_extra_flags(environment, link=False)
                extra_flags += self.get_compile_only_args()
            else:
                extra_flags += self.get_cross_extra_flags(environment, link=True)
        # Is a valid executable output for all toolchains and platforms
        binname += '.exe'
        # Write binary check source
        binary_name = os.path.join(work_dir, binname)
        with open(source_name, 'w') as ofile:
            ofile.write(code)
        # Compile sanity check
        cmdlist = self.exelist + extra_flags + [source_name] + self.get_output_args(binary_name)
        pc, stdo, stde = Popen_safe(cmdlist, cwd=work_dir)
        mlog.debug('Sanity check compiler command line:', ' '.join(cmdlist))
        mlog.debug('Sanity check compile stdout:')
        mlog.debug(stdo)
        mlog.debug('-----\nSanity check compile stderr:')
        mlog.debug(stde)
        mlog.debug('-----')
        if pc.returncode != 0:
            raise EnvironmentException('Compiler {0} can not compile programs.'.format(self.name_string()))
        # Run sanity check
        if self.is_cross:
            if self.exe_wrapper is None:
                # Can't check if the binaries run so we have to assume they do
                return
            cmdlist = self.exe_wrapper + [binary_name]
        else:
            cmdlist = [binary_name]
        mlog.debug('Running test binary command: ' + ' '.join(cmdlist))
        pe = subprocess.Popen(cmdlist)
        pe.wait()
        if pe.returncode != 0:
            raise EnvironmentException('Executables created by {0} compiler {1} are not runnable.'.format(self.language, self.name_string()))

    def sanity_check(self, work_dir, environment):
        code = 'int main(int argc, char **argv) { int class=0; return class; }\n'
        return self.sanity_check_impl(work_dir, environment, 'sanitycheckc.c', code)

    def has_header(self, hname, prefix, env, extra_args=None, dependencies=None):
        fargs = {'prefix': prefix, 'header': hname}
        code = '''{prefix}
        #ifdef __has_include
         #if !__has_include("{header}")
          #error "Header '{header}' could not be found"
         #endif
        #else
         #include <{header}>
        #endif'''
        return self.compiles(code.format(**fargs), env, extra_args,
                             dependencies, 'preprocess')

    def has_header_symbol(self, hname, symbol, prefix, env, extra_args=None, dependencies=None):
        fargs = {'prefix': prefix, 'header': hname, 'symbol': symbol}
        t = '''{prefix}
        #include <{header}>
        int main () {{
            /* If it's not defined as a macro, try to use as a symbol */
            #ifndef {symbol}
                {symbol};
            #endif
        }}'''
        return self.compiles(t.format(**fargs), env, extra_args, dependencies)

    def _get_compiler_check_args(self, env, extra_args, dependencies, mode='compile'):
        if extra_args is None:
            extra_args = []
        elif isinstance(extra_args, str):
            extra_args = [extra_args]
        if dependencies is None:
            dependencies = []
        elif not isinstance(dependencies, list):
            dependencies = [dependencies]
        # Collect compiler arguments
        args = CompilerArgs(self)
        for d in dependencies:
            # Add compile flags needed by dependencies
            args += d.get_compile_args()
            if mode == 'link':
                # Add link flags needed to find dependencies
                args += d.get_link_args()
        # Select a CRT if needed since we're linking
        if mode == 'link':
            args += self.get_linker_debug_crt_args()
        # Read c_args/cpp_args/etc from the cross-info file (if needed)
        args += self.get_cross_extra_flags(env, link=(mode == 'link'))
        if not self.is_cross:
            if mode == 'preprocess':
                # Add CPPFLAGS from the env.
                args += env.coredata.external_preprocess_args[self.language]
            elif mode == 'compile':
                # Add CFLAGS/CXXFLAGS/OBJCFLAGS/OBJCXXFLAGS from the env
                args += env.coredata.external_args[self.language]
            elif mode == 'link':
                # Add LDFLAGS from the env
                args += env.coredata.external_link_args[self.language]
        args += self.get_compiler_check_args()
        # extra_args must override all other arguments, so we add them last
        args += extra_args
        return args

    def compiles(self, code, env, extra_args=None, dependencies=None, mode='compile'):
        args = self._get_compiler_check_args(env, extra_args, dependencies, mode)
        # We only want to compile; not link
        with self.compile(code, args.to_native(), mode) as p:
            return p.returncode == 0

    def _links_wrapper(self, code, env, extra_args, dependencies):
        "Shares common code between self.links and self.run"
        args = self._get_compiler_check_args(env, extra_args, dependencies, mode='link')
        return self.compile(code, args)

    def links(self, code, env, extra_args=None, dependencies=None):
        with self._links_wrapper(code, env, extra_args, dependencies) as p:
            return p.returncode == 0

    def run(self, code, env, extra_args=None, dependencies=None):
        if self.is_cross and self.exe_wrapper is None:
            raise CrossNoRunException('Can not run test applications in this cross environment.')
        with self._links_wrapper(code, env, extra_args, dependencies) as p:
            if p.returncode != 0:
                mlog.debug('Could not compile test file %s: %d\n' % (
                    p.input_name,
                    p.returncode))
                return RunResult(False)
            if self.is_cross:
                cmdlist = self.exe_wrapper + [p.output_name]
            else:
                cmdlist = p.output_name
            try:
                pe, so, se = Popen_safe(cmdlist)
            except Exception as e:
                mlog.debug('Could not run: %s (error: %s)\n' % (cmdlist, e))
                return RunResult(False)

        mlog.debug('Program stdout:\n')
        mlog.debug(so)
        mlog.debug('Program stderr:\n')
        mlog.debug(se)
        return RunResult(True, pe.returncode, so, se)

    def _compile_int(self, expression, prefix, env, extra_args, dependencies):
        fargs = {'prefix': prefix, 'expression': expression}
        t = '''#include <stdio.h>
        {prefix}
        int main() {{ static int a[1-2*!({expression})]; a[0]=0; return 0; }}'''
        return self.compiles(t.format(**fargs), env, extra_args, dependencies)

    def cross_compute_int(self, expression, low, high, guess, prefix, env, extra_args, dependencies):
        if isinstance(guess, int):
            if self._compile_int('%s == %d' % (expression, guess), prefix, env, extra_args, dependencies):
                return guess

        cur = low
        while low < high:
            cur = int((low + high) / 2)
            if cur == low:
                break

            if self._compile_int('%s >= %d' % (expression, cur), prefix, env, extra_args, dependencies):
                low = cur
            else:
                high = cur

        if self._compile_int('%s == %d' % (expression, cur), prefix, env, extra_args, dependencies):
            return cur
        raise EnvironmentException('Cross-compile check overflowed')

    def compute_int(self, expression, low, high, guess, prefix, env, extra_args=None, dependencies=None):
        if extra_args is None:
            extra_args = []
        if self.is_cross:
            return self.cross_compute_int(expression, low, high, guess, prefix, env, extra_args, dependencies)
        fargs = {'prefix': prefix, 'expression': expression}
        t = '''#include<stdio.h>
        {prefix}
        int main(int argc, char **argv) {{
            printf("%ld\\n", (long)({expression}));
            return 0;
        }};'''
        res = self.run(t.format(**fargs), env, extra_args, dependencies)
        if not res.compiled:
            return -1
        if res.returncode != 0:
            raise EnvironmentException('Could not run compute_int test binary.')
        return int(res.stdout)

    def cross_sizeof(self, typename, prefix, env, extra_args=None, dependencies=None):
        if extra_args is None:
            extra_args = []
        fargs = {'prefix': prefix, 'type': typename}
        t = '''#include <stdio.h>
        {prefix}
        int main(int argc, char **argv) {{
            {type} something;
        }}'''
        if not self.compiles(t.format(**fargs), env, extra_args, dependencies):
            return -1
        return self.cross_compute_int('sizeof(%s)' % typename, 1, 128, None, prefix, env, extra_args, dependencies)

    def sizeof(self, typename, prefix, env, extra_args=None, dependencies=None):
        if extra_args is None:
            extra_args = []
        fargs = {'prefix': prefix, 'type': typename}
        if self.is_cross:
            return self.cross_sizeof(typename, prefix, env, extra_args, dependencies)
        t = '''#include<stdio.h>
        {prefix}
        int main(int argc, char **argv) {{
            printf("%ld\\n", (long)(sizeof({type})));
            return 0;
        }};'''
        res = self.run(t.format(**fargs), env, extra_args, dependencies)
        if not res.compiled:
            return -1
        if res.returncode != 0:
            raise EnvironmentException('Could not run sizeof test binary.')
        return int(res.stdout)

    def cross_alignment(self, typename, prefix, env, extra_args=None, dependencies=None):
        if extra_args is None:
            extra_args = []
        fargs = {'prefix': prefix, 'type': typename}
        t = '''#include <stdio.h>
        {prefix}
        int main(int argc, char **argv) {{
            {type} something;
        }}'''
        if not self.compiles(t.format(**fargs), env, extra_args, dependencies):
            return -1
        t = '''#include <stddef.h>
        {prefix}
        struct tmp {{
            char c;
            {type} target;
        }};'''
        return self.cross_compute_int('offsetof(struct tmp, target)', 1, 1024, None, t.format(**fargs), env, extra_args, dependencies)

    def alignment(self, typename, prefix, env, extra_args=None, dependencies=None):
        if extra_args is None:
            extra_args = []
        if self.is_cross:
            return self.cross_alignment(typename, prefix, env, extra_args, dependencies)
        fargs = {'prefix': prefix, 'type': typename}
        t = '''#include <stdio.h>
        #include <stddef.h>
        {prefix}
        struct tmp {{
            char c;
            {type} target;
        }};
        int main(int argc, char **argv) {{
            printf("%d", (int)offsetof(struct tmp, target));
            return 0;
        }}'''
        res = self.run(t.format(**fargs), env, extra_args, dependencies)
        if not res.compiled:
            raise EnvironmentException('Could not compile alignment test.')
        if res.returncode != 0:
            raise EnvironmentException('Could not run alignment test binary.')
        align = int(res.stdout)
        if align == 0:
            raise EnvironmentException('Could not determine alignment of %s. Sorry. You might want to file a bug.' % typename)
        return align

    def get_define(self, dname, prefix, env, extra_args, dependencies):
        delim = '"MESON_GET_DEFINE_DELIMITER"'
        fargs = {'prefix': prefix, 'define': dname, 'delim': delim}
        code = '''
        {prefix}
        #ifndef {define}
        # define {define}
        #endif
        {delim}\n{define}'''
        args = self._get_compiler_check_args(env, extra_args, dependencies,
                                             mode='preprocess').to_native()
        with self.compile(code.format(**fargs), args, 'preprocess') as p:
            if p.returncode != 0:
                raise EnvironmentException('Could not get define {!r}'.format(dname))
        # Get the preprocessed value after the delimiter,
        # minus the extra newline at the end
        return p.stdo.split(delim + '\n')[-1][:-1]

    @staticmethod
    def _no_prototype_templ():
        """
        Try to find the function without a prototype from a header by defining
        our own dummy prototype and trying to link with the C library (and
        whatever else the compiler links in by default). This is very similar
        to the check performed by Autoconf for AC_CHECK_FUNCS.
        """
        # Define the symbol to something else since it is defined by the
        # includes or defines listed by the user or by the compiler. This may
        # include, for instance _GNU_SOURCE which must be defined before
        # limits.h, which includes features.h
        # Then, undef the symbol to get rid of it completely.
        head = '''
        #define {func} meson_disable_define_of_{func}
        {prefix}
        #include <limits.h>
        #undef {func}
        '''
        # Override any GCC internal prototype and declare our own definition for
        # the symbol. Use char because that's unlikely to be an actual return
        # value for a function which ensures that we override the definition.
        head += '''
        #ifdef __cplusplus
        extern "C"
        #endif
        char {func} ();
        '''
        # The actual function call
        main = '''
        int main () {{
          return {func} ();
        }}'''
        return head, main

    @staticmethod
    def _have_prototype_templ():
        """
        Returns a head-er and main() call that uses the headers listed by the
        user for the function prototype while checking if a function exists.
        """
        # Add the 'prefix', aka defines, includes, etc that the user provides
        # This may include, for instance _GNU_SOURCE which must be defined
        # before limits.h, which includes features.h
        head = '{prefix}\n#include <limits.h>\n'
        # We don't know what the function takes or returns, so return it as an int.
        # Just taking the address or comparing it to void is not enough because
        # compilers are smart enough to optimize it away. The resulting binary
        # is not run so we don't care what the return value is.
        main = '''\nint main() {{
            void *a = (void*) &{func};
            long b = (long) a;
            return (int) b;
        }}'''
        return head, main

    def has_function(self, funcname, prefix, env, extra_args=None, dependencies=None):
        """
        First, this function looks for the symbol in the default libraries
        provided by the compiler (stdlib + a few others usually). If that
        fails, it checks if any of the headers specified in the prefix provide
        an implementation of the function, and if that fails, it checks if it's
        implemented as a compiler-builtin.
        """
        if extra_args is None:
            extra_args = []

        # Short-circuit if the check is already provided by the cross-info file
        varname = 'has function ' + funcname
        varname = varname.replace(' ', '_')
        if self.is_cross:
            val = env.cross_info.config['properties'].get(varname, None)
            if val is not None:
                if isinstance(val, bool):
                    return val
                raise EnvironmentException('Cross variable {0} is not a boolean.'.format(varname))

        fargs = {'prefix': prefix, 'func': funcname}

        # glibc defines functions that are not available on Linux as stubs that
        # fail with ENOSYS (such as e.g. lchmod). In this case we want to fail
        # instead of detecting the stub as a valid symbol.
        # We already included limits.h earlier to ensure that these are defined
        # for stub functions.
        stubs_fail = '''
        #if defined __stub_{func} || defined __stub___{func}
        fail fail fail this function is not going to work
        #endif
        '''

        # If we have any includes in the prefix supplied by the user, assume
        # that the user wants us to use the symbol prototype defined in those
        # includes. If not, then try to do the Autoconf-style check with
        # a dummy prototype definition of our own.
        # This is needed when the linker determines symbol availability from an
        # SDK based on the prototype in the header provided by the SDK.
        # Ignoring this prototype would result in the symbol always being
        # marked as available.
        if '#include' in prefix:
            head, main = self._have_prototype_templ()
        else:
            head, main = self._no_prototype_templ()
        templ = head + stubs_fail + main

        if self.links(templ.format(**fargs), env, extra_args, dependencies):
            return True

        # MSVC does not have compiler __builtin_-s.
        if self.get_id() == 'msvc':
            return False

        # Detect function as a built-in
        #
        # Some functions like alloca() are defined as compiler built-ins which
        # are inlined by the compiler and you can't take their address, so we
        # need to look for them differently. On nice compilers like clang, we
        # can just directly use the __has_builtin() macro.
        fargs['no_includes'] = '#include' not in prefix
        t = '''{prefix}
        int main() {{
        #ifdef __has_builtin
            #if !__has_builtin(__builtin_{func})
                #error "__builtin_{func} not found"
            #endif
        #elif ! defined({func})
            /* Check for __builtin_{func} only if no includes were added to the
             * prefix above, which means no definition of {func} can be found.
             * We would always check for this, but we get false positives on
             * MSYS2 if we do. Their toolchain is broken, but we can at least
             * give them a workaround. */
            #if {no_includes:d}
                __builtin_{func};
            #else
                #error "No definition for __builtin_{func} found in the prefix"
            #endif
        #endif
        }}'''
        return self.links(t.format(**fargs), env, extra_args, dependencies)

    def has_members(self, typename, membernames, prefix, env, extra_args=None, dependencies=None):
        if extra_args is None:
            extra_args = []
        fargs = {'prefix': prefix, 'type': typename, 'name': 'foo'}
        # Create code that accesses all members
        members = ''
        for member in membernames:
            members += '{}.{};\n'.format(fargs['name'], member)
        fargs['members'] = members
        t = '''{prefix}
        void bar() {{
            {type} {name};
            {members}
        }};'''
        return self.compiles(t.format(**fargs), env, extra_args, dependencies)

    def has_type(self, typename, prefix, env, extra_args, dependencies=None):
        fargs = {'prefix': prefix, 'type': typename}
        t = '''{prefix}
        void bar() {{
            sizeof({type});
        }};'''
        return self.compiles(t.format(**fargs), env, extra_args, dependencies)

    def symbols_have_underscore_prefix(self, env):
        '''
        Check if the compiler prefixes an underscore to global C symbols
        '''
        symbol_name = b'meson_uscore_prefix'
        code = '''#ifdef __cplusplus
        extern "C" {
        #endif
        void ''' + symbol_name.decode() + ''' () {}
        #ifdef __cplusplus
        }
        #endif
        '''
        args = self.get_cross_extra_flags(env, link=False)
        args += self.get_compiler_check_args()
        n = 'symbols_have_underscore_prefix'
        with self.compile(code, args, 'compile') as p:
            if p.returncode != 0:
                m = 'BUG: Unable to compile {!r} check: {}'
                raise RuntimeError(m.format(n, p.stdo))
            if not os.path.isfile(p.output_name):
                m = 'BUG: Can\'t find compiled test code for {!r} check'
                raise RuntimeError(m.format(n))
            with open(p.output_name, 'rb') as o:
                for line in o:
                    # Check if the underscore form of the symbol is somewhere
                    # in the output file.
                    if b'_' + symbol_name in line:
                        return True
                    # Else, check if the non-underscored form is present
                    elif symbol_name in line:
                        return False
        raise RuntimeError('BUG: {!r} check failed unexpectedly'.format(n))

    def find_library(self, libname, env, extra_dirs):
        # These libraries are either built-in or invalid
        if libname in self.ignore_libs:
            return []
        # First try if we can just add the library as -l.
        code = 'int main(int argc, char **argv) { return 0; }'
        if extra_dirs and isinstance(extra_dirs, str):
            extra_dirs = [extra_dirs]
        # Gcc + co seem to prefer builtin lib dirs to -L dirs.
        # Only try to find std libs if no extra dirs specified.
        if not extra_dirs:
            args = ['-l' + libname]
            if self.links(code, env, extra_args=args):
                return args
        # Not found? Try to find the library file itself.
        extra_dirs += self.get_library_dirs()
        suffixes = ['so', 'dylib', 'lib', 'dll', 'a']
        for d in extra_dirs:
            for suffix in suffixes:
                trial = os.path.join(d, 'lib' + libname + '.' + suffix)
                if os.path.isfile(trial):
                    return [trial]
                trial2 = os.path.join(d, libname + '.' + suffix)
                if os.path.isfile(trial2):
                    return [trial2]
        return None

    def thread_flags(self):
        return ['-pthread']

    def thread_link_flags(self):
        return ['-pthread']

    def has_multi_arguments(self, args, env):
        return self.compiles('int i;\n', env, extra_args=args)


class ClangCCompiler(ClangCompiler, CCompiler):
    def __init__(self, exelist, version, clang_type, is_cross, exe_wrapper=None):
        CCompiler.__init__(self, exelist, version, is_cross, exe_wrapper)
        ClangCompiler.__init__(self, clang_type)
        default_warn_args = ['-Wall', '-Winvalid-pch']
        self.warn_args = {'1': default_warn_args,
                          '2': default_warn_args + ['-Wextra'],
                          '3': default_warn_args + ['-Wextra', '-Wpedantic']}

    def get_options(self):
        return {'c_std': coredata.UserComboOption('c_std', 'C language standard to use',
                                                  ['none', 'c89', 'c99', 'c11',
                                                   'gnu89', 'gnu99', 'gnu11'],
                                                  'none')}

    def get_option_compile_args(self, options):
        args = []
        std = options['c_std']
        if std.value != 'none':
            args.append('-std=' + std.value)
        return args

    def get_option_link_args(self, options):
        return []


class GnuCCompiler(GnuCompiler, CCompiler):
    def __init__(self, exelist, version, gcc_type, is_cross, exe_wrapper=None, defines=None):
        CCompiler.__init__(self, exelist, version, is_cross, exe_wrapper)
        GnuCompiler.__init__(self, gcc_type, defines)
        default_warn_args = ['-Wall', '-Winvalid-pch']
        self.warn_args = {'1': default_warn_args,
                          '2': default_warn_args + ['-Wextra'],
                          '3': default_warn_args + ['-Wextra', '-Wpedantic']}

    def get_options(self):
        opts = {'c_std': coredata.UserComboOption('c_std', 'C language standard to use',
                                                  ['none', 'c89', 'c99', 'c11',
                                                   'gnu89', 'gnu99', 'gnu11'],
                                                  'none')}
        if self.gcc_type == GCC_MINGW:
            opts.update({
                'c_winlibs': coredata.UserStringArrayOption('c_winlibs', 'Standard Win libraries to link against',
                                                            gnu_winlibs), })
        return opts

    def get_option_compile_args(self, options):
        args = []
        std = options['c_std']
        if std.value != 'none':
            args.append('-std=' + std.value)
        return args

    def get_option_link_args(self, options):
        if self.gcc_type == GCC_MINGW:
            return options['c_winlibs'].value[:]
        return []

    def get_std_shared_lib_link_args(self):
        return ['-shared']


class IntelCCompiler(IntelCompiler, CCompiler):
    def __init__(self, exelist, version, icc_type, is_cross, exe_wrapper=None):
        CCompiler.__init__(self, exelist, version, is_cross, exe_wrapper)
        IntelCompiler.__init__(self, icc_type)
        self.lang_header = 'c-header'
        default_warn_args = ['-Wall', '-w3', '-diag-disable:remark', '-Wpch-messages']
        self.warn_args = {'1': default_warn_args,
                          '2': default_warn_args + ['-Wextra'],
                          '3': default_warn_args + ['-Wextra', '-Wpedantic']}

    def get_options(self):
        c_stds = ['c89', 'c99']
        g_stds = ['gnu89', 'gnu99']
        if version_compare(self.version, '>=16.0.0'):
            c_stds += ['c11']
        opts = {'c_std': coredata.UserComboOption('c_std', 'C language standard to use',
                                                  ['none'] + c_stds + g_stds,
                                                  'none')}
        return opts

    def get_option_compile_args(self, options):
        args = []
        std = options['c_std']
        if std.value != 'none':
            args.append('-std=' + std.value)
        return args

    def get_std_shared_lib_link_args(self):
        return ['-shared']

    def has_multi_arguments(self, args, env):
        return super().has_multi_arguments(args + ['-diag-error', '10006'], env)


class VisualStudioCCompiler(CCompiler):
    std_warn_args = ['/W3']
    std_opt_args = ['/O2']
    ignore_libs = ('m', 'c', 'pthread')

    def __init__(self, exelist, version, is_cross, exe_wrap, is_64):
        CCompiler.__init__(self, exelist, version, is_cross, exe_wrap)
        self.id = 'msvc'
        # /showIncludes is needed for build dependency tracking in Ninja
        # See: https://ninja-build.org/manual.html#_deps
        self.always_args = ['/nologo', '/showIncludes']
        self.warn_args = {'1': ['/W2'],
                          '2': ['/W3'],
                          '3': ['/W4']}
        self.base_options = ['b_pch'] # FIXME add lto, pgo and the like
        self.is_64 = is_64

    # Override CCompiler.get_always_args
    def get_always_args(self):
        return self.always_args

    def get_linker_debug_crt_args(self):
        """
        Arguments needed to select a debug crt for the linker

        Sometimes we need to manually select the CRT (C runtime) to use with
        MSVC. One example is when trying to link with static libraries since
        MSVC won't auto-select a CRT for us in that case and will error out
        asking us to select one.
        """
        return ['/MDd']

    def get_buildtype_args(self, buildtype):
        return msvc_buildtype_args[buildtype]

    def get_buildtype_linker_args(self, buildtype):
        return msvc_buildtype_linker_args[buildtype]

    def get_pch_suffix(self):
        return 'pch'

    def get_pch_name(self, header):
        chopped = os.path.split(header)[-1].split('.')[:-1]
        chopped.append(self.get_pch_suffix())
        pchname = '.'.join(chopped)
        return pchname

    def get_pch_use_args(self, pch_dir, header):
        base = os.path.split(header)[-1]
        pchname = self.get_pch_name(header)
        return ['/FI' + base, '/Yu' + base, '/Fp' + os.path.join(pch_dir, pchname)]

    def get_preprocess_only_args(self):
        return ['/EP']

    def get_compile_only_args(self):
        return ['/c']

    def get_no_optimization_args(self):
        return ['/Od']

    def get_output_args(self, target):
        if target.endswith('.exe'):
            return ['/Fe' + target]
        return ['/Fo' + target]

    def get_dependency_gen_args(self, outtarget, outfile):
        return []

    def get_linker_exelist(self):
        return ['link'] # FIXME, should have same path as compiler.

    def get_linker_always_args(self):
        return ['/nologo']

    def get_linker_output_args(self, outputname):
        return ['/OUT:' + outputname]

    def get_linker_search_args(self, dirname):
        return ['/LIBPATH:' + dirname]

    def get_pic_args(self):
        return [] # PIC is handled by the loader on Windows

    def get_std_shared_lib_link_args(self):
        return ['/DLL']

    def gen_vs_module_defs_args(self, defsfile):
        if not isinstance(defsfile, str):
            raise RuntimeError('Module definitions file should be str')
        # With MSVC, DLLs only export symbols that are explicitly exported,
        # so if a module defs file is specified, we use that to export symbols
        return ['/DEF:' + defsfile]

    def gen_pch_args(self, header, source, pchname):
        objname = os.path.splitext(pchname)[0] + '.obj'
        return objname, ['/Yc' + header, '/Fp' + pchname, '/Fo' + objname]

    def gen_import_library_args(self, implibname):
        "The name of the outputted import library"
        return ['/IMPLIB:' + implibname]

    def build_rpath_args(self, build_dir, from_dir, rpath_paths, build_rpath, install_rpath):
        return []

    # FIXME, no idea what these should be.
    def thread_flags(self):
        return []

    def thread_link_flags(self):
        return []

    def get_options(self):
        return {'c_winlibs': coredata.UserStringArrayOption('c_winlibs',
                                                            'Windows libs to link against.',
                                                            msvc_winlibs)
                }

    def get_option_link_args(self, options):
        return options['c_winlibs'].value[:]

    @classmethod
    def unix_args_to_native(cls, args):
        result = []
        for i in args:
            # -mms-bitfields is specific to MinGW-GCC
            # -pthread is only valid for GCC
            if i in ('-mms-bitfields', '-pthread'):
                continue
            if i.startswith('-L'):
                i = '/LIBPATH:' + i[2:]
            # Translate GNU-style -lfoo library name to the import library
            elif i.startswith('-l'):
                name = i[2:]
                if name in cls.ignore_libs:
                    # With MSVC, these are provided by the C runtime which is
                    # linked in by default
                    continue
                else:
                    i = name + '.lib'
            # -pthread in link flags is only used on Linux
            elif i == '-pthread':
                continue
            result.append(i)
        return result

    def get_werror_args(self):
        return ['/WX']

    def get_include_args(self, path, is_system):
        if path == '':
            path = '.'
        # msvc does not have a concept of system header dirs.
        return ['-I' + path]

    # Visual Studio is special. It ignores some arguments it does not
    # understand and you can't tell it to error out on those.
    # http://stackoverflow.com/questions/15259720/how-can-i-make-the-microsoft-c-compiler-treat-unknown-flags-as-errors-rather-t
    def has_multi_arguments(self, args, env):
        warning_text = '9002'
        code = 'int i;\n'
        (fd, srcname) = tempfile.mkstemp(suffix='.' + self.default_suffix)
        os.close(fd)
        with open(srcname, 'w') as ofile:
            ofile.write(code)
        # Read c_args/cpp_args/etc from the cross-info file (if needed)
        extra_args = self.get_cross_extra_flags(env, link=False)
        extra_args += self.get_compile_only_args()
        commands = self.exelist + args + extra_args + [srcname]
        mlog.debug('Running VS compile:')
        mlog.debug('Command line: ', ' '.join(commands))
        mlog.debug('Code:\n', code)
        p, stdo, stde = Popen_safe(commands, cwd=os.path.split(srcname)[0])
        if p.returncode != 0:
            return False
        return not(warning_text in stde or warning_text in stdo)

    def get_compile_debugfile_args(self, rel_obj, pch=False):
        pdbarr = rel_obj.split('.')[:-1]
        pdbarr += ['pdb']
        args = ['/Fd' + '.'.join(pdbarr)]
        # When generating a PDB file with PCH, all compile commands write
        # to the same PDB file. Hence, we need to serialize the PDB
        # writes using /FS since we do parallel builds. This slows down the
        # build obviously, which is why we only do this when PCH is on.
        # This was added in Visual Studio 2013 (MSVC 18.0). Before that it was
        # always on: https://msdn.microsoft.com/en-us/library/dn502518.aspx
        if pch and version_compare(self.version, '>=18.0'):
            args = ['/FS'] + args
        return args

    def get_link_debugfile_args(self, targetfile):
        pdbarr = targetfile.split('.')[:-1]
        pdbarr += ['pdb']
        return ['/DEBUG', '/PDB:' + '.'.join(pdbarr)]

    def get_link_whole_for(self, args):
        # Only since VS2015
        if not isinstance(args, list):
            args = [args]
        return ['/WHOLEARCHIVE:' + x for x in args]

    def get_instruction_set_args(self, instruction_set):
        if self.is_64:
            return vs64_instruction_set_args.get(instruction_set, None)
        if self.version.split('.')[0] == '16' and instruction_set == 'avx':
            # VS documentation says that this exists and should work, but
            # it does not. The headers do not contain AVX intrinsics
            # and the can not be called.
            return None
        return vs32_instruction_set_args.get(instruction_set, None)


