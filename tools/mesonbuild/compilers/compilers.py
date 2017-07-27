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

import contextlib, os.path, re, tempfile

from ..linkers import StaticLinker
from .. import coredata
from .. import mlog
from .. import mesonlib
from ..mesonlib import EnvironmentException, MesonException, version_compare, Popen_safe

"""This file contains the data files of all compilers Meson knows
about. To support a new compiler, add its information below.
Also add corresponding autodetection code in environment.py."""

header_suffixes = ('h', 'hh', 'hpp', 'hxx', 'H', 'ipp', 'moc', 'vapi', 'di')
obj_suffixes = ('o', 'obj', 'res')
lib_suffixes = ('a', 'lib', 'dll', 'dylib', 'so')
# Mapping of language to suffixes of files that should always be in that language
# This means we can't include .h headers here since they could be C, C++, ObjC, etc.
lang_suffixes = {
    'c': ('c',),
    'cpp': ('cpp', 'cc', 'cxx', 'c++', 'hh', 'hpp', 'ipp', 'hxx'),
    # f90, f95, f03, f08 are for free-form fortran ('f90' recommended)
    # f, for, ftn, fpp are for fixed-form fortran ('f' or 'for' recommended)
    'fortran': ('f90', 'f95', 'f03', 'f08', 'f', 'for', 'ftn', 'fpp'),
    'd': ('d', 'di'),
    'objc': ('m',),
    'objcpp': ('mm',),
    'rust': ('rs',),
    'vala': ('vala', 'vapi', 'gs'),
    'cs': ('cs',),
    'swift': ('swift',),
    'java': ('java',),
}
cpp_suffixes = lang_suffixes['cpp'] + ('h',)
c_suffixes = lang_suffixes['c'] + ('h',)
# List of languages that can be linked with C code directly by the linker
# used in build.py:process_compilers() and build.py:get_dynamic_linker()
clike_langs = ('objcpp', 'objc', 'd', 'cpp', 'c', 'fortran',)
clike_suffixes = ()
for _l in clike_langs:
    clike_suffixes += lang_suffixes[_l]
clike_suffixes += ('h', 'll', 's')

# XXX: Use this in is_library()?
soregex = re.compile(r'.*\.so(\.[0-9]+)?(\.[0-9]+)?(\.[0-9]+)?$')

# All these are only for C-like languages; see `clike_langs` above.

def sort_clike(lang):
    '''
    Sorting function to sort the list of languages according to
    reversed(compilers.clike_langs) and append the unknown langs in the end.
    The purpose is to prefer C over C++ for files that can be compiled by
    both such as assembly, C, etc. Also applies to ObjC, ObjC++, etc.
    '''
    if lang not in clike_langs:
        return 1
    return -clike_langs.index(lang)

def is_header(fname):
    if hasattr(fname, 'fname'):
        fname = fname.fname
    suffix = fname.split('.')[-1]
    return suffix in header_suffixes

def is_source(fname):
    if hasattr(fname, 'fname'):
        fname = fname.fname
    suffix = fname.split('.')[-1].lower()
    return suffix in clike_suffixes

def is_assembly(fname):
    if hasattr(fname, 'fname'):
        fname = fname.fname
    return fname.split('.')[-1].lower() == 's'

def is_llvm_ir(fname):
    if hasattr(fname, 'fname'):
        fname = fname.fname
    return fname.split('.')[-1] == 'll'

def is_object(fname):
    if hasattr(fname, 'fname'):
        fname = fname.fname
    suffix = fname.split('.')[-1]
    return suffix in obj_suffixes

def is_library(fname):
    if hasattr(fname, 'fname'):
        fname = fname.fname
    suffix = fname.split('.')[-1]
    return suffix in lib_suffixes

gnulike_buildtype_args = {'plain': [],
                          # -O0 is passed for improved debugging information with gcc
                          # See https://github.com/mesonbuild/meson/pull/509
                          'debug': ['-O0', '-g'],
                          'debugoptimized': ['-O2', '-g'],
                          'release': ['-O3'],
                          'minsize': ['-Os', '-g']}

msvc_buildtype_args = {'plain': [],
                       'debug': ["/MDd", "/ZI", "/Ob0", "/Od", "/RTC1"],
                       'debugoptimized': ["/MD", "/Zi", "/O2", "/Ob1"],
                       'release': ["/MD", "/O2", "/Ob2"],
                       'minsize': ["/MD", "/Zi", "/Os", "/Ob1"],
                       }

apple_buildtype_linker_args = {'plain': [],
                               'debug': [],
                               'debugoptimized': [],
                               'release': [],
                               'minsize': [],
                               }

gnulike_buildtype_linker_args = {'plain': [],
                                 'debug': [],
                                 'debugoptimized': [],
                                 'release': ['-Wl,-O1'],
                                 'minsize': [],
                                 }

msvc_buildtype_linker_args = {'plain': [],
                              'debug': [],
                              'debugoptimized': [],
                              'release': [],
                              'minsize': ['/INCREMENTAL:NO'],
                              }

java_buildtype_args = {'plain': [],
                       'debug': ['-g'],
                       'debugoptimized': ['-g'],
                       'release': [],
                       'minsize': [],
                       }

rust_buildtype_args = {'plain': [],
                       'debug': ['-C', 'debuginfo=2'],
                       'debugoptimized': ['-C', 'debuginfo=2', '-C', 'opt-level=2'],
                       'release': ['-C', 'opt-level=3'],
                       'minsize': [], # In a future release: ['-C', 'opt-level=s'],
                       }

d_gdc_buildtype_args = {'plain': [],
                        'debug': ['-g', '-O0'],
                        'debugoptimized': ['-g', '-O'],
                        'release': ['-O3', '-frelease'],
                        'minsize': [],
                        }

d_ldc_buildtype_args = {'plain': [],
                        'debug': ['-g', '-O0'],
                        'debugoptimized': ['-g', '-O'],
                        'release': ['-O3', '-release'],
                        'minsize': [],
                        }

d_dmd_buildtype_args = {'plain': [],
                        'debug': ['-g'],
                        'debugoptimized': ['-g', '-O'],
                        'release': ['-O', '-release'],
                        'minsize': [],
                        }

mono_buildtype_args = {'plain': [],
                       'debug': ['-debug'],
                       'debugoptimized': ['-debug', '-optimize+'],
                       'release': ['-optimize+'],
                       'minsize': [],
                       }

swift_buildtype_args = {'plain': [],
                        'debug': ['-g'],
                        'debugoptimized': ['-g', '-O'],
                        'release': ['-O'],
                        'minsize': [],
                        }

gnu_winlibs = ['-lkernel32', '-luser32', '-lgdi32', '-lwinspool', '-lshell32',
               '-lole32', '-loleaut32', '-luuid', '-lcomdlg32', '-ladvapi32']

msvc_winlibs = ['kernel32.lib', 'user32.lib', 'gdi32.lib',
                'winspool.lib', 'shell32.lib', 'ole32.lib', 'oleaut32.lib',
                'uuid.lib', 'comdlg32.lib', 'advapi32.lib']

gnu_color_args = {'auto': ['-fdiagnostics-color=auto'],
                  'always': ['-fdiagnostics-color=always'],
                  'never': ['-fdiagnostics-color=never'],
                  }

clang_color_args = {'auto': ['-Xclang', '-fcolor-diagnostics'],
                    'always': ['-Xclang', '-fcolor-diagnostics'],
                    'never': ['-Xclang', '-fno-color-diagnostics'],
                    }

base_options = {'b_pch': coredata.UserBooleanOption('b_pch', 'Use precompiled headers', True),
                'b_lto': coredata.UserBooleanOption('b_lto', 'Use link time optimization', False),
                'b_sanitize': coredata.UserComboOption('b_sanitize',
                                                       'Code sanitizer to use',
                                                       ['none', 'address', 'thread', 'undefined', 'memory', 'address,undefined'],
                                                       'none'),
                'b_lundef': coredata.UserBooleanOption('b_lundef', 'Use -Wl,--no-undefined when linking', True),
                'b_asneeded': coredata.UserBooleanOption('b_asneeded', 'Use -Wl,--as-needed when linking', True),
                'b_pgo': coredata.UserComboOption('b_pgo', 'Use profile guided optimization',
                                                  ['off', 'generate', 'use'],
                                                  'off'),
                'b_coverage': coredata.UserBooleanOption('b_coverage',
                                                         'Enable coverage tracking.',
                                                         False),
                'b_colorout': coredata.UserComboOption('b_colorout', 'Use colored output',
                                                       ['auto', 'always', 'never'],
                                                       'always'),
                'b_ndebug': coredata.UserBooleanOption('b_ndebug',
                                                       'Disable asserts',
                                                       False),
                'b_staticpic': coredata.UserBooleanOption('b_staticpic',
                                                          'Build static libraries as position independent',
                                                          True),
                }

gnulike_instruction_set_args = {'mmx': ['-mmmx'],
                                'sse': ['-msse'],
                                'sse2': ['-msse2'],
                                'sse3': ['-msse3'],
                                'ssse3': ['-mssse3'],
                                'sse41': ['-msse4.1'],
                                'sse42': ['-msse4.2'],
                                'avx': ['-mavx'],
                                'avx2': ['-mavx2'],
                                'neon': ['-mfpu=neon'],
                                }

vs32_instruction_set_args = {'mmx': ['/arch:SSE'], # There does not seem to be a flag just for MMX
                             'sse': ['/arch:SSE'],
                             'sse2': ['/arch:SSE2'],
                             'sse3': ['/arch:AVX'], # VS leaped from SSE2 directly to AVX.
                             'sse41': ['/arch:AVX'],
                             'sse42': ['/arch:AVX'],
                             'avx': ['/arch:AVX'],
                             'avx2': ['/arch:AVX2'],
                             'neon': None,
}

# The 64 bit compiler defaults to /arch:avx.
vs64_instruction_set_args = {'mmx': ['/arch:AVX'],
                             'sse': ['/arch:AVX'],
                             'sse2': ['/arch:AVX'],
                             'sse3': ['/arch:AVX'],
                             'ssse3': ['/arch:AVX'],
                             'sse41': ['/arch:AVX'],
                             'sse42': ['/arch:AVX'],
                             'avx': ['/arch:AVX'],
                             'avx2': ['/arch:AVX2'],
                             'neon': None,
                             }


def sanitizer_compile_args(value):
    if value == 'none':
        return []
    args = ['-fsanitize=' + value]
    if value == 'address':
        args.append('-fno-omit-frame-pointer')
    return args

def sanitizer_link_args(value):
    if value == 'none':
        return []
    args = ['-fsanitize=' + value]
    return args

def get_base_compile_args(options, compiler):
    args = []
    # FIXME, gcc/clang specific.
    try:
        if options['b_lto'].value:
            args.append('-flto')
    except KeyError:
        pass
    try:
        args += compiler.get_colorout_args(options['b_colorout'].value)
    except KeyError:
        pass
    try:
        args += sanitizer_compile_args(options['b_sanitize'].value)
    except KeyError:
        pass
    try:
        pgo_val = options['b_pgo'].value
        if pgo_val == 'generate':
            args.append('-fprofile-generate')
        elif pgo_val == 'use':
            args.append('-fprofile-use')
    except KeyError:
        pass
    try:
        if options['b_coverage'].value:
            args += compiler.get_coverage_args()
    except KeyError:
        pass
    try:
        if options['b_ndebug'].value:
            args += ['-DNDEBUG']
    except KeyError:
        pass
    return args

def get_base_link_args(options, linker, is_shared_module):
    args = []
    # FIXME, gcc/clang specific.
    try:
        if options['b_lto'].value:
            args.append('-flto')
    except KeyError:
        pass
    try:
        args += sanitizer_link_args(options['b_sanitize'].value)
    except KeyError:
        pass
    try:
        pgo_val = options['b_pgo'].value
        if pgo_val == 'generate':
            args.append('-fprofile-generate')
        elif pgo_val == 'use':
            args.append('-fprofile-use')
    except KeyError:
        pass
    try:
        if not is_shared_module and 'b_lundef' in linker.base_options and options['b_lundef'].value:
            args.append('-Wl,--no-undefined')
    except KeyError:
        pass
    try:
        if 'b_asneeded' in linker.base_options and options['b_asneeded'].value:
            args.append('-Wl,--as-needed')
    except KeyError:
        pass
    try:
        if options['b_coverage'].value:
            args += linker.get_coverage_link_args()
    except KeyError:
        pass
    return args

class CrossNoRunException(MesonException):
    pass

class RunResult:
    def __init__(self, compiled, returncode=999, stdout='UNDEFINED', stderr='UNDEFINED'):
        self.compiled = compiled
        self.returncode = returncode
        self.stdout = stdout
        self.stderr = stderr

class CompilerArgs(list):
    '''
    Class derived from list() that manages a list of compiler arguments. Should
    be used while constructing compiler arguments from various sources. Can be
    operated with ordinary lists, so this does not need to be used everywhere.

    All arguments must be inserted and stored in GCC-style (-lfoo, -Idir, etc)
    and can converted to the native type of each compiler by using the
    .to_native() method to which you must pass an instance of the compiler or
    the compiler class.

    New arguments added to this class (either with .append(), .extend(), or +=)
    are added in a way that ensures that they override previous arguments.
    For example:

    >>> a = ['-Lfoo', '-lbar']
    >>> a += ['-Lpho', '-lbaz']
    >>> print(a)
    ['-Lpho', '-Lfoo', '-lbar', '-lbaz']

    Arguments will also be de-duped if they can be de-duped safely.

    Note that because of all this, this class is not commutative and does not
    preserve the order of arguments if it is safe to not. For example:
    >>> ['-Ifoo', '-Ibar'] + ['-Ifez', '-Ibaz', '-Werror']
    ['-Ifez', '-Ibaz', '-Ifoo', '-Ibar', '-Werror']
    >>> ['-Ifez', '-Ibaz', '-Werror'] + ['-Ifoo', '-Ibar']
    ['-Ifoo', '-Ibar', '-Ifez', '-Ibaz', '-Werror']

    '''
    # NOTE: currently this class is only for C-like compilers, but it can be
    # extended to other languages easily. Just move the following to the
    # compiler class and initialize when self.compiler is set.

    # Arg prefixes that override by prepending instead of appending
    prepend_prefixes = ('-I', '-L')
    # Arg prefixes and args that must be de-duped by returning 2
    dedup2_prefixes = ('-I', '-L', '-D', '-U')
    dedup2_suffixes = ()
    dedup2_args = ()
    # Arg prefixes and args that must be de-duped by returning 1
    dedup1_prefixes = ('-l',)
    dedup1_suffixes = ('.lib', '.dll', '.so', '.dylib', '.a')
    # Match a .so of the form path/to/libfoo.so.0.1.0
    # Only UNIX shared libraries require this. Others have a fixed extension.
    dedup1_regex = re.compile(r'([\/\\]|\A)lib.*\.so(\.[0-9]+)?(\.[0-9]+)?(\.[0-9]+)?$')
    dedup1_args = ('-c', '-S', '-E', '-pipe', '-pthread')
    compiler = None

    def _check_args(self, args):
        cargs = []
        if len(args) > 2:
            raise TypeError("CompilerArgs() only accepts at most 2 arguments: "
                            "The compiler, and optionally an initial list")
        elif not args:
            return cargs
        elif len(args) == 1:
            if isinstance(args[0], (Compiler, StaticLinker)):
                self.compiler = args[0]
            else:
                raise TypeError("you must pass a Compiler instance as one of "
                                "the arguments")
        elif len(args) == 2:
            if isinstance(args[0], (Compiler, StaticLinker)):
                self.compiler = args[0]
                cargs = args[1]
            elif isinstance(args[1], (Compiler, StaticLinker)):
                cargs = args[0]
                self.compiler = args[1]
            else:
                raise TypeError("you must pass a Compiler instance as one of "
                                "the two arguments")
        else:
            raise AssertionError('Not reached')
        return cargs

    def __init__(self, *args):
        super().__init__(self._check_args(args))

    @classmethod
    def _can_dedup(cls, arg):
        '''
        Returns whether the argument can be safely de-duped. This is dependent
        on three things:

        a) Whether an argument can be 'overriden' by a later argument.  For
           example, -DFOO defines FOO and -UFOO undefines FOO. In this case, we
           can safely remove the previous occurance and add a new one. The same
           is true for include paths and library paths with -I and -L. For
           these we return `2`. See `dedup2_prefixes` and `dedup2_args`.
        b) Arguments that once specified cannot be undone, such as `-c` or
           `-pipe`. New instances of these can be completely skipped. For these
           we return `1`. See `dedup1_prefixes` and `dedup1_args`.
        c) Whether it matters where or how many times on the command-line
           a particular argument is present. This can matter for symbol
           resolution in static or shared libraries, so we cannot de-dup or
           reorder them. For these we return `0`. This is the default.

        In addition to these, we handle library arguments specially.
        With GNU ld, we surround library arguments with -Wl,--start/end-group
        to recursively search for symbols in the libraries. This is not needed
        with other linkers.
        '''

        # A standalone argument must never be deduplicated because it is
        # defined by what comes _after_ it. Thus dedupping this:
        # -D FOO -D BAR
        # would yield either
        # -D FOO BAR
        # or
        # FOO -D BAR
        # both of which are invalid.
        if arg in cls.dedup2_prefixes:
            return 0
        if arg in cls.dedup2_args or \
           arg.startswith(cls.dedup2_prefixes) or \
           arg.endswith(cls.dedup2_suffixes):
            return 2
        if arg in cls.dedup1_args or \
           arg.startswith(cls.dedup1_prefixes) or \
           arg.endswith(cls.dedup1_suffixes) or \
           re.search(cls.dedup1_regex, arg):
            return 1
        return 0

    @classmethod
    def _should_prepend(cls, arg):
        if arg.startswith(cls.prepend_prefixes):
            return True
        return False

    def to_native(self):
        # Check if we need to add --start/end-group for circular dependencies
        # between static libraries, and for recursively searching for symbols
        # needed by static libraries that are provided by object files or
        # shared libraries.
        if get_compiler_uses_gnuld(self.compiler):
            global soregex
            group_start = -1
            for each in self:
                if not each.startswith('-l') and not each.endswith('.a') and \
                   not soregex.match(each):
                    continue
                i = self.index(each)
                if group_start < 0:
                    # First occurance of a library
                    group_start = i
            if group_start >= 0:
                # Last occurance of a library
                self.insert(i + 1, '-Wl,--end-group')
                self.insert(group_start, '-Wl,--start-group')
        return self.compiler.unix_args_to_native(self)

    def append_direct(self, arg):
        '''
        Append the specified argument without any reordering or de-dup
        '''
        super().append(arg)

    def extend_direct(self, iterable):
        '''
        Extend using the elements in the specified iterable without any
        reordering or de-dup
        '''
        super().extend(iterable)

    def __add__(self, args):
        new = CompilerArgs(self, self.compiler)
        new += args
        return new

    def __iadd__(self, args):
        '''
        Add two CompilerArgs while taking into account overriding of arguments
        and while preserving the order of arguments as much as possible
        '''
        pre = []
        post = []
        if not isinstance(args, list):
            raise TypeError('can only concatenate list (not "{}") to list'.format(args))
        for arg in args:
            # If the argument can be de-duped, do it either by removing the
            # previous occurance of it and adding a new one, or not adding the
            # new occurance.
            dedup = self._can_dedup(arg)
            if dedup == 1:
                # Argument already exists and adding a new instance is useless
                if arg in self or arg in pre or arg in post:
                    continue
            if dedup == 2:
                # Remove all previous occurances of the arg and add it anew
                if arg in self:
                    self.remove(arg)
                if arg in pre:
                    pre.remove(arg)
                if arg in post:
                    post.remove(arg)
            if self._should_prepend(arg):
                pre.append(arg)
            else:
                post.append(arg)
        # Insert at the beginning
        self[:0] = pre
        # Append to the end
        super().__iadd__(post)
        return self

    def __radd__(self, args):
        new = CompilerArgs(args, self.compiler)
        new += self
        return new

    def __mul__(self, args):
        raise TypeError("can't multiply compiler arguments")

    def __imul__(self, args):
        raise TypeError("can't multiply compiler arguments")

    def __rmul__(self, args):
        raise TypeError("can't multiply compiler arguments")

    def append(self, arg):
        self.__iadd__([arg])

    def extend(self, args):
        self.__iadd__(args)

class Compiler:
    # Libraries to ignore in find_library() since they are provided by the
    # compiler or the C library. Currently only used for MSVC.
    ignore_libs = ()

    def __init__(self, exelist, version):
        if isinstance(exelist, str):
            self.exelist = [exelist]
        elif isinstance(exelist, list):
            self.exelist = exelist
        else:
            raise TypeError('Unknown argument to Compiler')
        # In case it's been overriden by a child class already
        if not hasattr(self, 'file_suffixes'):
            self.file_suffixes = lang_suffixes[self.language]
        if not hasattr(self, 'can_compile_suffixes'):
            self.can_compile_suffixes = set(self.file_suffixes)
        self.default_suffix = self.file_suffixes[0]
        self.version = version
        self.base_options = []

    def __repr__(self):
        repr_str = "<{0}: v{1} `{2}`>"
        return repr_str.format(self.__class__.__name__, self.version,
                               ' '.join(self.exelist))

    def can_compile(self, src):
        if hasattr(src, 'fname'):
            src = src.fname
        suffix = os.path.splitext(src)[1].lower()
        if suffix and suffix[1:] in self.can_compile_suffixes:
            return True
        return False

    def get_id(self):
        return self.id

    def get_language(self):
        return self.language

    def get_display_language(self):
        return self.language.capitalize()

    def get_default_suffix(self):
        return self.default_suffix

    def get_exelist(self):
        return self.exelist[:]

    def get_builtin_define(self, *args, **kwargs):
        raise EnvironmentException('%s does not support get_builtin_define.' % self.id)

    def has_builtin_define(self, *args, **kwargs):
        raise EnvironmentException('%s does not support has_builtin_define.' % self.id)

    def get_always_args(self):
        return []

    def get_linker_always_args(self):
        return []

    def gen_import_library_args(self, implibname):
        """
        Used only on Windows for libraries that need an import library.
        This currently means C, C++, Fortran.
        """
        return []

    def get_options(self):
        return {} # build afresh every time

    def get_option_compile_args(self, options):
        return []

    def get_option_link_args(self, options):
        return []

    def has_header(self, *args, **kwargs):
        raise EnvironmentException('Language %s does not support header checks.' % self.get_display_language())

    def has_header_symbol(self, *args, **kwargs):
        raise EnvironmentException('Language %s does not support header symbol checks.' % self.get_display_language())

    def compiles(self, *args, **kwargs):
        raise EnvironmentException('Language %s does not support compile checks.' % self.get_display_language())

    def links(self, *args, **kwargs):
        raise EnvironmentException('Language %s does not support link checks.' % self.get_display_language())

    def run(self, *args, **kwargs):
        raise EnvironmentException('Language %s does not support run checks.' % self.get_display_language())

    def sizeof(self, *args, **kwargs):
        raise EnvironmentException('Language %s does not support sizeof checks.' % self.get_display_language())

    def alignment(self, *args, **kwargs):
        raise EnvironmentException('Language %s does not support alignment checks.' % self.get_display_language())

    def has_function(self, *args, **kwargs):
        raise EnvironmentException('Language %s does not support function checks.' % self.get_display_language())

    @classmethod
    def unix_args_to_native(cls, args):
        "Always returns a copy that can be independently mutated"
        return args[:]

    def find_library(self, *args, **kwargs):
        raise EnvironmentException('Language {} does not support library finding.'.format(self.get_display_language()))

    def get_library_dirs(self):
        return []

    def has_argument(self, arg, env):
        return self.has_multi_arguments([arg], env)

    def has_multi_arguments(self, args, env):
        raise EnvironmentException(
            'Language {} does not support has_multi_arguments.'.format(
                self.get_display_language()))

    def get_cross_extra_flags(self, environment, link):
        extra_flags = []
        if self.is_cross and environment:
            if 'properties' in environment.cross_info.config:
                props = environment.cross_info.config['properties']
                lang_args_key = self.language + '_args'
                extra_flags += props.get(lang_args_key, [])
                lang_link_args_key = self.language + '_link_args'
                if link:
                    extra_flags += props.get(lang_link_args_key, [])
        return extra_flags

    def _get_compile_output(self, dirname, mode):
        # In pre-processor mode, the output is sent to stdout and discarded
        if mode == 'preprocess':
            return None
        # Extension only matters if running results; '.exe' is
        # guaranteed to be executable on every platform.
        if mode == 'link':
            suffix = 'exe'
        else:
            suffix = 'obj'
        return os.path.join(dirname, 'output.' + suffix)

    @contextlib.contextmanager
    def compile(self, code, extra_args=None, mode='link'):
        if extra_args is None:
            extra_args = []
        try:
            with tempfile.TemporaryDirectory() as tmpdirname:
                if isinstance(code, str):
                    srcname = os.path.join(tmpdirname,
                                           'testfile.' + self.default_suffix)
                    with open(srcname, 'w') as ofile:
                        ofile.write(code)
                elif isinstance(code, mesonlib.File):
                    srcname = code.fname
                output = self._get_compile_output(tmpdirname, mode)

                # Construct the compiler command-line
                commands = CompilerArgs(self)
                commands.append(srcname)
                commands += extra_args
                commands += self.get_always_args()
                if mode == 'compile':
                    commands += self.get_compile_only_args()
                # Preprocess mode outputs to stdout, so no output args
                if mode == 'preprocess':
                    commands += self.get_preprocess_only_args()
                else:
                    commands += self.get_output_args(output)
                # Generate full command-line with the exelist
                commands = self.get_exelist() + commands.to_native()
                mlog.debug('Running compile:')
                mlog.debug('Working directory: ', tmpdirname)
                mlog.debug('Command line: ', ' '.join(commands), '\n')
                mlog.debug('Code:\n', code)
                p, p.stdo, p.stde = Popen_safe(commands, cwd=tmpdirname)
                mlog.debug('Compiler stdout:\n', p.stdo)
                mlog.debug('Compiler stderr:\n', p.stde)
                p.input_name = srcname
                p.output_name = output
                yield p
        except (PermissionError, OSError):
            # On Windows antivirus programs and the like hold on to files so
            # they can't be deleted. There's not much to do in this case. Also,
            # catch OSError because the directory is then no longer empty.
            pass

    def get_colorout_args(self, colortype):
        return []

    # Some compilers (msvc) write debug info to a separate file.
    # These args specify where it should be written.
    def get_compile_debugfile_args(self, rel_obj, **kwargs):
        return []

    def get_link_debugfile_args(self, rel_obj):
        return []

    def get_std_shared_lib_link_args(self):
        return []

    def get_std_shared_module_link_args(self):
        return self.get_std_shared_lib_link_args()

    def get_link_whole_for(self, args):
        if isinstance(args, list) and not args:
            return []
        raise EnvironmentException('Language %s does not support linking whole archives.' % self.get_display_language())

    # Compiler arguments needed to enable the given instruction set.
    # May be [] meaning nothing needed or None meaning the given set
    # is not supported.
    def get_instruction_set_args(self, instruction_set):
        return None

    def build_unix_rpath_args(self, build_dir, from_dir, rpath_paths, build_rpath, install_rpath):
        if not rpath_paths and not install_rpath and not build_rpath:
            return []
        # The rpaths we write must be relative, because otherwise
        # they have different length depending on the build
        # directory. This breaks reproducible builds.
        rel_rpaths = []
        for p in rpath_paths:
            if p == from_dir:
                relative = '' # relpath errors out in this case
            else:
                relative = os.path.relpath(os.path.join(build_dir, p), os.path.join(build_dir, from_dir))
            rel_rpaths.append(relative)
        paths = ':'.join([os.path.join('$ORIGIN', p) for p in rel_rpaths])
        # Build_rpath is used as-is (it is usually absolute).
        if build_rpath != '':
            paths += ':' + build_rpath
        if len(paths) < len(install_rpath):
            padding = 'X' * (len(install_rpath) - len(paths))
            if not paths:
                paths = padding
            else:
                paths = paths + ':' + padding
        args = ['-Wl,-rpath,' + paths]
        if get_compiler_is_linuxlike(self):
            # Rpaths to use while linking must be absolute. These are not
            # written to the binary. Needed only with GNU ld:
            # https://sourceware.org/bugzilla/show_bug.cgi?id=16936
            # Not needed on Windows or other platforms that don't use RPATH
            # https://github.com/mesonbuild/meson/issues/1897
            lpaths = ':'.join([os.path.join(build_dir, p) for p in rpath_paths])
            args += ['-Wl,-rpath-link,' + lpaths]
        return args


GCC_STANDARD = 0
GCC_OSX = 1
GCC_MINGW = 2
GCC_CYGWIN = 3

CLANG_STANDARD = 0
CLANG_OSX = 1
CLANG_WIN = 2
# Possibly clang-cl?

ICC_STANDARD = 0
ICC_OSX = 1
ICC_WIN = 2

def get_gcc_soname_args(gcc_type, prefix, shlib_name, suffix, path, soversion, is_shared_module):
    if soversion is None:
        sostr = ''
    else:
        sostr = '.' + soversion
    if gcc_type in (GCC_STANDARD, GCC_MINGW, GCC_CYGWIN):
        # Might not be correct for mingw but seems to work.
        return ['-Wl,-soname,%s%s.%s%s' % (prefix, shlib_name, suffix, sostr)]
    elif gcc_type == GCC_OSX:
        if is_shared_module:
            return []
        return ['-install_name', os.path.join(path, 'lib' + shlib_name + '.dylib')]
    else:
        raise RuntimeError('Not implemented yet.')

def get_compiler_is_linuxlike(compiler):
    if (getattr(compiler, 'gcc_type', None) == GCC_STANDARD) or \
       (getattr(compiler, 'clang_type', None) == CLANG_STANDARD) or \
       (getattr(compiler, 'icc_type', None) == ICC_STANDARD):
        return True
    return False

def get_compiler_uses_gnuld(c):
    # FIXME: Perhaps we should detect the linker in the environment?
    # FIXME: Assumes that *BSD use GNU ld, but they might start using lld soon
    if (getattr(c, 'gcc_type', None) in (GCC_STANDARD, GCC_MINGW, GCC_CYGWIN)) or \
       (getattr(c, 'clang_type', None) in (CLANG_STANDARD, CLANG_WIN)) or \
       (getattr(c, 'icc_type', None) in (ICC_STANDARD, ICC_WIN)):
        return True
    return False

def get_largefile_args(compiler):
    '''
    Enable transparent large-file-support for 32-bit UNIX systems
    '''
    if get_compiler_is_linuxlike(compiler):
        # Enable large-file support unconditionally on all platforms other
        # than macOS and Windows. macOS is now 64-bit-only so it doesn't
        # need anything special, and Windows doesn't have automatic LFS.
        # You must use the 64-bit counterparts explicitly.
        # glibc, musl, and uclibc, and all BSD libcs support this. On Android,
        # support for transparent LFS is available depending on the version of
        # Bionic: https://github.com/android/platform_bionic#32-bit-abi-bugs
        # https://code.google.com/p/android/issues/detail?id=64613
        #
        # If this breaks your code, fix it! It's been 20+ years!
        return ['-D_FILE_OFFSET_BITS=64']
        # We don't enable -D_LARGEFILE64_SOURCE since that enables
        # transitionary features and must be enabled by programs that use
        # those features explicitly.
    return []


class GnuCompiler:
    # Functionality that is common to all GNU family compilers.
    def __init__(self, gcc_type, defines):
        self.id = 'gcc'
        self.gcc_type = gcc_type
        self.defines = defines or {}
        self.base_options = ['b_pch', 'b_lto', 'b_pgo', 'b_sanitize', 'b_coverage',
                             'b_colorout', 'b_ndebug', 'b_staticpic']
        if self.gcc_type != GCC_OSX:
            self.base_options.append('b_lundef')
            self.base_options.append('b_asneeded')
        # All GCC backends can do assembly
        self.can_compile_suffixes.add('s')

    def get_colorout_args(self, colortype):
        if mesonlib.version_compare(self.version, '>=4.9.0'):
            return gnu_color_args[colortype][:]
        return []

    def get_warn_args(self, level):
        args = super().get_warn_args(level)
        if mesonlib.version_compare(self.version, '<4.8.0') and '-Wpedantic' in args:
            # -Wpedantic was added in 4.8.0
            # https://gcc.gnu.org/gcc-4.8/changes.html
            args[args.index('-Wpedantic')] = '-pedantic'
        return args

    def has_builtin_define(self, define):
        return define in self.defines

    def get_builtin_define(self, define):
        if define in self.defines:
            return self.defines[define]

    def get_pic_args(self):
        if self.gcc_type in (GCC_CYGWIN, GCC_MINGW, GCC_OSX):
            return [] # On Window and OS X, pic is always on.
        return ['-fPIC']

    def get_buildtype_args(self, buildtype):
        return gnulike_buildtype_args[buildtype]

    def get_buildtype_linker_args(self, buildtype):
        if self.gcc_type == GCC_OSX:
            return apple_buildtype_linker_args[buildtype]
        return gnulike_buildtype_linker_args[buildtype]

    def get_pch_suffix(self):
        return 'gch'

    def split_shlib_to_parts(self, fname):
        return os.path.split(fname)[0], fname

    def get_soname_args(self, prefix, shlib_name, suffix, path, soversion, is_shared_module):
        return get_gcc_soname_args(self.gcc_type, prefix, shlib_name, suffix, path, soversion, is_shared_module)

    def get_std_shared_lib_link_args(self):
        return ['-shared']

    def get_link_whole_for(self, args):
        return ['-Wl,--whole-archive'] + args + ['-Wl,--no-whole-archive']

    def gen_vs_module_defs_args(self, defsfile):
        if not isinstance(defsfile, str):
            raise RuntimeError('Module definitions file should be str')
        # On Windows targets, .def files may be specified on the linker command
        # line like an object file.
        if self.gcc_type in (GCC_CYGWIN, GCC_MINGW):
            return [defsfile]
        # For other targets, discard the .def file.
        return []

    def get_gui_app_args(self):
        if self.gcc_type in (GCC_CYGWIN, GCC_MINGW):
            return ['-mwindows']
        return []

    def get_instruction_set_args(self, instruction_set):
        return gnulike_instruction_set_args.get(instruction_set, None)


class ClangCompiler:
    def __init__(self, clang_type):
        self.id = 'clang'
        self.clang_type = clang_type
        self.base_options = ['b_pch', 'b_lto', 'b_pgo', 'b_sanitize', 'b_coverage',
                             'b_ndebug', 'b_staticpic', 'b_colorout']
        if self.clang_type != CLANG_OSX:
            self.base_options.append('b_lundef')
            self.base_options.append('b_asneeded')
        # All Clang backends can do assembly and LLVM IR
        self.can_compile_suffixes.update(['ll', 's'])

    def get_pic_args(self):
        if self.clang_type in (CLANG_WIN, CLANG_OSX):
            return [] # On Window and OS X, pic is always on.
        return ['-fPIC']

    def get_colorout_args(self, colortype):
        return clang_color_args[colortype][:]

    def get_buildtype_args(self, buildtype):
        return gnulike_buildtype_args[buildtype]

    def get_buildtype_linker_args(self, buildtype):
        if self.clang_type == CLANG_OSX:
            return apple_buildtype_linker_args[buildtype]
        return gnulike_buildtype_linker_args[buildtype]

    def get_pch_suffix(self):
        return 'pch'

    def get_pch_use_args(self, pch_dir, header):
        # Workaround for Clang bug http://llvm.org/bugs/show_bug.cgi?id=15136
        # This flag is internal to Clang (or at least not documented on the man page)
        # so it might change semantics at any time.
        return ['-include-pch', os.path.join(pch_dir, self.get_pch_name(header))]

    def get_soname_args(self, prefix, shlib_name, suffix, path, soversion, is_shared_module):
        if self.clang_type == CLANG_STANDARD:
            gcc_type = GCC_STANDARD
        elif self.clang_type == CLANG_OSX:
            gcc_type = GCC_OSX
        elif self.clang_type == CLANG_WIN:
            gcc_type = GCC_MINGW
        else:
            raise MesonException('Unreachable code when converting clang type to gcc type.')
        return get_gcc_soname_args(gcc_type, prefix, shlib_name, suffix, path, soversion, is_shared_module)

    def has_multi_arguments(self, args, env):
        return super().has_multi_arguments(
            ['-Werror=unknown-warning-option', '-Werror=unused-command-line-argument'] + args,
            env)

    def has_function(self, funcname, prefix, env, extra_args=None, dependencies=None):
        if extra_args is None:
            extra_args = []
        # Starting with XCode 8, we need to pass this to force linker
        # visibility to obey OS X and iOS minimum version targets with
        # -mmacosx-version-min, -miphoneos-version-min, etc.
        # https://github.com/Homebrew/homebrew-core/issues/3727
        if self.clang_type == CLANG_OSX and version_compare(self.version, '>=8.0'):
            extra_args.append('-Wl,-no_weak_imports')
        return super().has_function(funcname, prefix, env, extra_args, dependencies)

    def get_std_shared_module_link_args(self):
        if self.clang_type == CLANG_OSX:
            return ['-bundle', '-Wl,-undefined,dynamic_lookup']
        return ['-shared']

    def get_link_whole_for(self, args):
        if self.clang_type == CLANG_OSX:
            result = []
            for a in args:
                result += ['-Wl,-force_load', a]
            return result
        return ['-Wl,--whole-archive'] + args + ['-Wl,--no-whole-archive']

    def get_instruction_set_args(self, instruction_set):
        return gnulike_instruction_set_args.get(instruction_set, None)


# Tested on linux for ICC 14.0.3, 15.0.6, 16.0.4, 17.0.1
class IntelCompiler:
    def __init__(self, icc_type):
        self.id = 'intel'
        self.icc_type = icc_type
        self.lang_header = 'none'
        self.base_options = ['b_pch', 'b_lto', 'b_pgo', 'b_sanitize', 'b_coverage',
                             'b_colorout', 'b_ndebug', 'b_staticpic', 'b_lundef', 'b_asneeded']
        # Assembly
        self.can_compile_suffixes.add('s')

    def get_pic_args(self):
        return ['-fPIC']

    def get_buildtype_args(self, buildtype):
        return gnulike_buildtype_args[buildtype]

    def get_buildtype_linker_args(self, buildtype):
        return gnulike_buildtype_linker_args[buildtype]

    def get_pch_suffix(self):
        return 'pchi'

    def get_pch_use_args(self, pch_dir, header):
        return ['-pch', '-pch_dir', os.path.join(pch_dir), '-x',
                self.lang_header, '-include', header, '-x', 'none']

    def get_pch_name(self, header_name):
        return os.path.split(header_name)[-1] + '.' + self.get_pch_suffix()

    def split_shlib_to_parts(self, fname):
        return os.path.split(fname)[0], fname

    def get_soname_args(self, prefix, shlib_name, suffix, path, soversion, is_shared_module):
        if self.icc_type == ICC_STANDARD:
            gcc_type = GCC_STANDARD
        elif self.icc_type == ICC_OSX:
            gcc_type = GCC_OSX
        elif self.icc_type == ICC_WIN:
            gcc_type = GCC_MINGW
        else:
            raise MesonException('Unreachable code when converting icc type to gcc type.')
        return get_gcc_soname_args(gcc_type, prefix, shlib_name, suffix, path, soversion, is_shared_module)

    def get_std_shared_lib_link_args(self):
        # FIXME: Don't know how icc works on OSX
        # if self.icc_type == ICC_OSX:
        #     return ['-bundle']
        return ['-shared']
