# Copyright 2012-2014 The Meson development team

# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at

#     http://www.apache.org/licenses/LICENSE-2.0

# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from . import environment
from . import dependencies
from . import mlog
import copy, os, re
from .mesonlib import File, flatten, MesonException, stringlistify, classify_unity_sources
from .environment import for_windows, for_darwin
from .compilers import is_object, clike_langs, lang_suffixes

known_basic_kwargs = {'install' : True,
                      'c_pch' : True,
                      'cpp_pch' : True,
                      'c_args' : True,
                      'cpp_args' : True,
                      'cs_args' : True,
                      'vala_args' : True,
                      'fortran_args' : True,
                      'd_args' : True,
                      'java_args' : True,
                      'link_args' : True,
                      'link_depends': True,
                      'link_with' : True,
                      'include_directories': True,
                      'dependencies' : True,
                      'install_dir' : True,
                      'main_class' : True,
                      'gui_app' : True,
                      'extra_files' : True,
                      'install_rpath' : True,
                      'resources' : True,
                      'sources' : True,
                      'objects' : True,
                      'native' : True,
                     }

# These contain kwargs supported by both static and shared libraries. These are
# combined here because a library() call might be shared_library() or
# static_library() at runtime based on the configuration.
# FIXME: Find a way to pass that info down here so we can have proper target
# kwargs checking when specifically using shared_library() or static_library().
known_lib_kwargs = known_basic_kwargs.copy()
known_lib_kwargs.update({'version' : True, # Only for shared libs
                         'soversion' : True, # Only for shared libs
                         'name_prefix' : True,
                         'name_suffix' : True,
                         'vs_module_defs' : True, # Only for shared libs
                         'vala_header': True,
                         'vala_vapi': True,
                         'vala_gir' : True,
                         'pic' : True, # Only for static libs
                        })


class InvalidArguments(MesonException):
    pass

class Build:
    """A class that holds the status of one build including
    all dependencies and so on.
    """

    def __init__(self, environment):
        self.project_name = 'name of master project'
        self.project_version = None
        self.environment = environment
        self.projects = {}
        self.targets = {}
        self.compilers = {}
        self.cross_compilers = {}
        self.global_args = {}
        self.projects_args = {}
        self.global_link_args = {}
        self.projects_link_args = {}
        self.tests = []
        self.benchmarks = []
        self.headers = []
        self.man = []
        self.data = []
        self.static_linker = None
        self.static_cross_linker = None
        self.subprojects = {}
        self.install_scripts = []
        self.postconf_scripts = []
        self.install_dirs = []
        self.dep_manifest_name = None
        self.dep_manifest = {}
        self.cross_stdlibs = {}

    def add_compiler(self, compiler):
        if self.static_linker is None and compiler.needs_static_linker():
            self.static_linker = self.environment.detect_static_linker(compiler)
        lang = compiler.get_language()
        if lang not in self.compilers:
            self.compilers[lang] = compiler

    def add_cross_compiler(self, compiler):
        if len(self.cross_compilers) == 0:
            self.static_cross_linker = self.environment.detect_static_linker(compiler)
        lang = compiler.get_language()
        if lang not in self.cross_compilers:
            self.cross_compilers[lang] = compiler

    def get_project(self):
        return self.projects['']

    def get_targets(self):
        return self.targets

    def get_tests(self):
        return self.tests

    def get_benchmarks(self):
        return self.benchmarks

    def get_headers(self):
        return self.headers

    def get_man(self):
        return self.man

    def get_data(self):
        return self.data

    def get_install_subdirs(self):
        return self.install_dirs

    def get_global_args(self, compiler):
        return self.global_args.get(compiler.get_language(), [])

    def get_project_args(self, compiler, project):
        args = self.projects_args.get(project)
        if not args:
            return []
        return args.get(compiler.get_language(), [])

    def get_global_link_args(self, compiler):
        return self.global_link_args.get(compiler.get_language(), [])

    def get_project_link_args(self, compiler, project):
        link_args = self.projects_link_args.get(project)
        if not link_args:
            return []

        return link_args.get(compiler.get_language(), [])

class IncludeDirs():
    def __init__(self, curdir, dirs, is_system, extra_build_dirs=None):
        self.curdir = curdir
        self.incdirs = dirs
        self.is_system = is_system
        # Interpreter has validated that all given directories
        # actually exist.
        if extra_build_dirs is None:
            self.extra_build_dirs = []
        else:
            self.extra_build_dirs = extra_build_dirs

    def get_curdir(self):
        return self.curdir

    def get_incdirs(self):
        return self.incdirs

    def get_extra_build_dirs(self):
        return self.extra_build_dirs

class ExtractedObjects():
    '''
    Holds a list of sources for which the objects must be extracted
    '''
    def __init__(self, target, srclist, is_unity):
        self.target = target
        self.srclist = srclist
        if is_unity:
            self.check_unity_compatible()

    def __repr__(self):
        r = '<{0} {1!r}: {2}>'
        return r.format(self.__class__.__name__, self.target.name, self.srclist)

    def check_unity_compatible(self):
        # Figure out if the extracted object list is compatible with a Unity
        # build. When we're doing a Unified build, we go through the sources,
        # and create a single source file from each subset of the sources that
        # can be compiled with a specific compiler. Then we create one object
        # from each unified source file.
        # If the list of sources for which we want objects is the same as the
        # list of sources that go into each unified build, we're good.
        srclist_set = set(self.srclist)
        # Objects for all the sources are required, so we're compatible
        if srclist_set == set(self.target.sources):
            return
        # Check if the srclist is a subset (of the target's sources) that is
        # going to form a unified source file and a single object
        compsrcs = classify_unity_sources(self.target.compilers.values(),
                                          self.target.sources)
        for srcs in compsrcs.values():
            if srclist_set == set(srcs):
                return
        msg = 'Single object files can not be extracted in Unity builds. ' \
              'You can only extract all the object files at once.'
        raise MesonException(msg)

    def get_want_all_objects(self):
        return self.want_all_objects

class EnvironmentVariables():
    def __init__(self):
        self.envvars = []

    def get_value(self, name, values, kwargs):
        separator = kwargs.get('separator', os.pathsep)

        value = ''
        for var in values:
            value += separator + var
        return separator, value.strip(separator)

    def set(self, env, name, values, kwargs):
        return self.get_value(name, values, kwargs)[1]

    def append(self, env, name, values, kwargs):
        sep, value = self.get_value(name, values, kwargs)
        if name in env:
            return env[name] + sep + value
        return value

    def prepend(self, env, name, values, kwargs):
        sep, value = self.get_value(name, values, kwargs)
        if name in env:
            return  value + sep + env[name]

        return value

    def get_env(self, full_env):
        env = {}
        for method, name, values, kwargs in self.envvars:
            env[name] = method(full_env, name, values, kwargs)
        return env


class BuildTarget():
    def __init__(self, name, subdir, subproject, is_cross, sources, objects, environment, kwargs):
        self.name = name
        self.subdir = subdir
        self.subproject = subproject # Can not be calculated from subdir as subproject dirname can be changed per project.
        self.is_cross = is_cross
        self.is_unity = environment.coredata.get_builtin_option('unity')
        self.environment = environment
        self.sources = []
        self.compilers = {}
        self.objects = []
        self.external_deps = []
        self.include_dirs = []
        self.link_targets = []
        self.link_depends = []
        self.name_prefix_set = False
        self.name_suffix_set = False
        self.filename = 'no_name'
        # The file with debugging symbols
        self.debug_filename = None
        self.need_install = False
        self.pch = {}
        self.extra_args = {}
        self.generated = []
        self.extra_files = []
        # Sources can be:
        # 1. Pre-existing source files in the source tree
        # 2. Pre-existing sources generated by configure_file in the build tree
        # 3. Sources files generated by another target or a Generator
        self.process_sourcelist(sources)
        # Objects can be:
        # 1. Pre-existing objects provided by the user with the `objects:` kwarg
        # 2. Compiled objects created by and extracted from another target
        self.process_objectlist(objects)
        self.process_kwargs(kwargs, environment)
        self.check_unknown_kwargs(kwargs)
        if len(self.sources) == 0 and \
            len(self.generated) == 0 and \
            len(self.objects) == 0:
            raise InvalidArguments('Build target %s has no sources.' % name)
        self.process_compilers()
        self.validate_sources()

    def __repr__(self):
        repr_str = "<{0} {1}: {2}>"
        return repr_str.format(self.__class__.__name__, self.get_id(), self.filename)

    def get_id(self):
        # This ID must also be a valid file name on all OSs.
        # It should also avoid shell metacharacters for obvious
        # reasons.
        base = self.name + self.type_suffix()
        if self.subproject == '':
            return base
        return self.subproject + '@@' + base

    def check_unknown_kwargs(self, kwargs):
        # Override this method in derived classes that have more
        # keywords.
        self.check_unknown_kwargs_int(kwargs, known_basic_kwargs)

    def check_unknown_kwargs_int(self, kwargs, known_kwargs):
        unknowns = []
        for k in kwargs:
            if k not in known_kwargs:
                unknowns.append(k)
        if len(unknowns) > 0:
            mlog.warning('Unknown keyword argument(s) in target %s: %s.' %
                         (self.name, ', '.join(unknowns)))

    def process_objectlist(self, objects):
        assert(isinstance(objects, list))
        for s in objects:
            if hasattr(s, 'held_object'):
                s = s.held_object
            if isinstance(s, (str, File, ExtractedObjects)):
                self.objects.append(s)
            elif isinstance(s, (GeneratedList, CustomTarget)):
                msg =  'Generated files are not allowed in the \'objects\' kwarg ' + \
                    'for target {!r}.\nIt is meant only for '.format(self.name) + \
                    'pre-built object files that are shipped with the\nsource ' + \
                    'tree. Try adding it in the list of sources.'
                raise InvalidArguments(msg)
            else:
                msg = 'Bad object of type {!r} in target {!r}.'.format(type(s).__name__, self.name)
                raise InvalidArguments(msg)

    def process_sourcelist(self, sources):
        if not isinstance(sources, list):
            sources = [sources]
        added_sources = {} # If the same source is defined multiple times, use it only once.
        for s in sources:
            # Holder unpacking. Ugly.
            if hasattr(s, 'held_object'):
                s = s.held_object
            if isinstance(s, File):
                if s not in added_sources:
                    self.sources.append(s)
                    added_sources[s] = True
            elif isinstance(s, (GeneratedList, CustomTarget)):
                self.generated.append(s)
            else:
                msg = 'Bad source of type {!r} in target {!r}.'.format(type(s).__name__, self.name)
                raise InvalidArguments(msg)

    @staticmethod
    def can_compile_sources(compiler, sources):
        for s in sources:
            if compiler.can_compile(s):
                return True
        return False

    @staticmethod
    def can_compile_remove_sources(compiler, sources):
        removed = False
        for s in sources[:]:
            if compiler.can_compile(s):
                sources.remove(s)
                removed = True
        return removed

    def process_compilers(self):
        '''
        Populate self.compilers, which is the list of compilers that this
        target will use for compiling all its sources.
        We also add compilers that were used by extracted objects to simplify
        dynamic linker determination.
        '''
        if len(self.sources) + len(self.generated) + len(self.objects) == 0:
            return
        # Populate list of compilers
        if self.is_cross:
            compilers = self.environment.coredata.cross_compilers
        else:
            compilers = self.environment.coredata.compilers
        # Pre-existing sources
        sources = list(self.sources)
        # All generated sources
        for gensrc in self.generated:
            for s in gensrc.get_outputs():
                # Generated objects can't be compiled, so don't use them for
                # compiler detection. If our target only has generated objects,
                # we will fall back to using the first c-like compiler we find,
                # which is what we need.
                if not is_object(s):
                    sources.append(s)
        # Sources that were used to create our extracted objects
        for o in self.objects:
            if not isinstance(o, ExtractedObjects):
                continue
            for s in o.srclist:
                # Don't add Vala sources since that will pull in the Vala
                # compiler even though we will never use it since we are
                # dealing with compiled C code.
                if not s.endswith(lang_suffixes['vala']):
                    sources.append(s)
        if sources:
            # Add compilers based on the above sources
            for lang, compiler in compilers.items():
                # We try to be conservative because sometimes people add files
                # in the list of sources that we can't determine the type based
                # just on the suffix.
                if self.can_compile_sources(compiler, sources):
                    self.compilers[lang] = compiler
        else:
            # No source files, target consists of only object files of unknown
            # origin. Just add the first clike compiler that we have and hope
            # that it can link these objects
            for lang in clike_langs:
                if lang in compilers:
                    self.compilers[lang] = compilers[lang]
                    break
        # If all our sources are Vala, our target also needs the C compiler but
        # it won't get added above.
        if 'vala' in self.compilers and 'c' not in self.compilers:
            self.compilers['c'] = compilers['c']

    def validate_sources(self):
        if len(self.sources) == 0:
            return
        for lang in ('cs', 'java'):
            if lang in self.compilers:
                check_sources = list(self.sources)
                compiler = self.compilers[lang]
                if not self.can_compile_remove_sources(compiler, check_sources):
                    m = 'No {} sources found in target {!r}'.format(lang, self.name)
                    raise InvalidArguments(m)
                if check_sources:
                    m = '{0} targets can only contain {0} files:\n'.format(lang.capitalize())
                    m += '\n'.join([repr(c) for c in check_sources])
                    raise InvalidArguments(m)
                # CSharp and Java targets can't contain any other file types
                assert(len(self.compilers) == 1)
                return
        if 'rust' in self.compilers:
            firstname = self.sources[0]
            if isinstance(firstname, File):
                firstname = firstname.fname
            first = os.path.split(firstname)[1]
            (base, suffix) = os.path.splitext(first)
            if suffix != '.rs' or self.name != base:
                raise InvalidArguments('In Rust targets, the first source file must be named projectname.rs.')

    def get_original_kwargs(self):
        return self.kwargs

    def unpack_holder(self, d):
        if not isinstance(d, list):
            d = [d]
        newd = []
        for i in d:
            if hasattr(i, 'held_object'):
                newd.append(i.held_object)
            else:
                newd.append(i)
        return newd

    def copy_kwargs(self, kwargs):
        self.kwargs = copy.copy(kwargs)
        # This sucks quite badly. Arguments
        # are holders but they can't be pickled
        # so unpack those known.
        if 'dependencies' in self.kwargs:
            self.kwargs['dependencies'] = self.unpack_holder(self.kwargs['dependencies'])
        if 'link_with' in self.kwargs:
            self.kwargs['link_with'] = self.unpack_holder(self.kwargs['link_with'])

    def extract_objects(self, srclist):
        obj_src = []
        for src in srclist:
            if not isinstance(src, str):
                raise MesonException('Object extraction arguments must be strings.')
            src = File(False, self.subdir, src)
            if src not in self.sources:
                raise MesonException('Tried to extract unknown source %s.' % src)
            obj_src.append(src)
        return ExtractedObjects(self, obj_src, self.is_unity)

    def extract_all_objects(self):
        return ExtractedObjects(self, self.sources, self.is_unity)

    def get_all_link_deps(self):
        return self.get_transitive_link_deps()

    def get_transitive_link_deps(self):
        result = []
        for i in self.link_targets:
            result += i.get_all_link_deps()
        return result

    def get_custom_install_dir(self):
        return self.custom_install_dir

    def process_kwargs(self, kwargs, environment):
        self.copy_kwargs(kwargs)
        kwargs.get('modules', [])
        self.need_install = kwargs.get('install', self.need_install)
        llist = kwargs.get('link_with', [])
        if not isinstance(llist, list):
            llist = [llist]
        for linktarget in llist:
            # Sorry for this hack. Keyword targets are kept in holders
            # in kwargs. Unpack here without looking at the exact type.
            if hasattr(linktarget, "held_object"):
                linktarget = linktarget.held_object
            self.link(linktarget)
        c_pchlist = kwargs.get('c_pch', [])
        if not isinstance(c_pchlist, list):
            c_pchlist = [c_pchlist]
        self.add_pch('c', c_pchlist)
        cpp_pchlist = kwargs.get('cpp_pch', [])
        if not isinstance(cpp_pchlist, list):
            cpp_pchlist = [cpp_pchlist]
        self.add_pch('cpp', cpp_pchlist)
        clist = kwargs.get('c_args', [])
        if not isinstance(clist, list):
            clist = [clist]
        self.add_compiler_args('c', clist)
        cpplist = kwargs.get('cpp_args', [])
        if not isinstance(cpplist, list):
            cpplist = [cpplist]
        self.add_compiler_args('cpp', cpplist)
        cslist = kwargs.get('cs_args', [])
        if not isinstance(cslist, list):
            cslist = [cslist]
        self.add_compiler_args('cs', cslist)
        valalist = kwargs.get('vala_args', [])
        if not isinstance(valalist, list):
            valalist = [valalist]
        self.add_compiler_args('vala', valalist)
        fortranlist = kwargs.get('fortran_args', [])
        if not isinstance(fortranlist, list):
            fortranlist = [fortranlist]
        self.add_compiler_args('fortran', fortranlist)
        if not isinstance(self, Executable):
            self.vala_header = kwargs.get('vala_header', self.name + '.h')
            self.vala_vapi = kwargs.get('vala_vapi', self.name + '.vapi')
        self.vala_gir = kwargs.get('vala_gir', None)
        dlist = stringlistify(kwargs.get('d_args', []))
        self.add_compiler_args('d', dlist)
        self.link_args = kwargs.get('link_args', [])
        if not isinstance(self.link_args, list):
            self.link_args = [self.link_args]
        for i in self.link_args:
            if not isinstance(i, str):
                raise InvalidArguments('Link_args arguments must be strings.')
        self.link_depends = kwargs.get('link_depends', [])
        if not isinstance(self.link_depends, list):
            self.link_depends = [self.link_depends]
        for i in self.link_depends:
            if not isinstance(i, str):
                raise InvalidArguments('Link_depends arguments must be strings.')
        inclist = kwargs.get('include_directories', [])
        if not isinstance(inclist, list):
            inclist = [inclist]
        self.add_include_dirs(inclist)
        deplist = kwargs.get('dependencies', [])
        if not isinstance(deplist, list):
            deplist = [deplist]
        self.add_deps(deplist)
        self.custom_install_dir = kwargs.get('install_dir', None)
        if self.custom_install_dir is not None:
            if not isinstance(self.custom_install_dir, str):
                raise InvalidArguments('Custom_install_dir must be a string')
        main_class = kwargs.get('main_class', '')
        if not isinstance(main_class, str):
            raise InvalidArguments('Main class must be a string')
        self.main_class = main_class
        if isinstance(self, Executable):
            self.gui_app = kwargs.get('gui_app', False)
            if not isinstance(self.gui_app, bool):
                raise InvalidArguments('Argument gui_app must be boolean.')
        elif 'gui_app' in kwargs:
            raise InvalidArguments('Argument gui_app can only be used on executables.')
        extra_files = kwargs.get('extra_files', [])
        if isinstance(extra_files, str):
            extra_files = [extra_files]
        for i in extra_files:
            if not isinstance(i, str):
                raise InvalidArguments('Arguments to extra_files must be strings.')
            trial = os.path.join(environment.get_source_dir(), self.subdir, i)
            if not(os.path.isfile(trial)):
                raise InvalidArguments('Tried to add non-existing extra file %s.' % i)
        self.extra_files = extra_files
        self.install_rpath = kwargs.get('install_rpath', '')
        if not isinstance(self.install_rpath, str):
            raise InvalidArguments('Install_rpath is not a string.')
        resources = kwargs.get('resources', [])
        if not isinstance(resources, list):
            resources = [resources]
        for r in resources:
            if not isinstance(r, str):
                raise InvalidArguments('Resource argument is not a string.')
            trial = os.path.join(environment.get_source_dir(), self.subdir, r)
            if not os.path.isfile(trial):
                raise InvalidArguments('Tried to add non-existing resource %s.' % r)
        self.resources = resources
        if 'name_prefix' in kwargs:
            name_prefix = kwargs['name_prefix']
            if isinstance(name_prefix, list):
                if len(name_prefix) != 0:
                    raise InvalidArguments('name_prefix array must be empty to signify null.')
            elif not isinstance(name_prefix, str):
                raise InvalidArguments('name_prefix must be a string.')
            self.prefix = name_prefix
            self.name_prefix_set = True
        if 'name_suffix' in kwargs:
            name_suffix = kwargs['name_suffix']
            if isinstance(name_suffix, list):
                if len(name_suffix) != 0:
                    raise InvalidArguments('name_suffix array must be empty to signify null.')
            else:
                if not isinstance(name_suffix, str):
                    raise InvalidArguments('name_suffix must be a string.')
                self.suffix = name_suffix
                self.name_suffix_set = True
        if isinstance(self, StaticLibrary):
            # You can't disable PIC on OS X. The compiler ignores -fno-PIC.
            # PIC is always on for Windows (all code is position-independent
            # since library loading is done differently)
            if for_darwin(self.is_cross, self.environment) or for_windows(self.is_cross, self.environment):
                self.pic = True
            elif '-fPIC' in clist + cpplist:
                mlog.warning("Use the 'pic' kwarg instead of passing -fPIC manually to static library {!r}".format(self.name))
                self.pic = True
            else:
                self.pic = kwargs.get('pic', False)
                if not isinstance(self.pic, bool):
                    raise InvalidArguments('Argument pic to static library {!r} must be boolean'.format(self.name))

    def get_subdir(self):
        return self.subdir

    def get_filename(self):
        return self.filename

    def get_outputs(self):
        return [self.filename]

    def get_debug_filename(self):
        """
        The name of the file that contains debugging symbols for this target

        Returns None if there are no debugging symbols or if they are embedded
        in the filename itself
        """
        return self.debug_filename

    def get_extra_args(self, language):
        return self.extra_args.get(language, [])

    def get_dependencies(self):
        transitive_deps = []
        for t in self.link_targets:
            transitive_deps.append(t)
            if isinstance(t, StaticLibrary):
                transitive_deps += t.get_dependencies()
        return transitive_deps

    def get_basename(self):
        return self.name

    def get_source_subdir(self):
        return self.subdir

    def get_sources(self):
        return self.sources

    def get_objects(self):
        return self.objects

    def get_generated_sources(self):
        return self.generated

    def should_install(self):
        return self.need_install

    def has_pch(self):
        return len(self.pch) > 0

    def get_pch(self, language):
        try:
            return self.pch[language]
        except KeyError:
            return[]

    def get_include_dirs(self):
        return self.include_dirs

    def add_deps(self, deps):
        if not isinstance(deps, list):
            deps = [deps]
        for dep in deps:
            if hasattr(dep, 'held_object'):
                dep = dep.held_object
            if isinstance(dep, dependencies.InternalDependency):
                # Those parts that are internal.
                self.process_sourcelist(dep.sources)
                self.add_include_dirs(dep.include_directories)
                for l in dep.libraries:
                    self.link(l)
                # Those parts that are external.
                extpart = dependencies.InternalDependency('undefined',
                                                          [],
                                                          dep.compile_args,
                                                          dep.link_args,
                                                          [], [], [])
                self.external_deps.append(extpart)
                # Deps of deps.
                self.add_deps(dep.ext_deps)
            elif isinstance(dep, dependencies.Dependency):
                self.external_deps.append(dep)
                self.process_sourcelist(dep.get_sources())
            else:
                # This is a bit of a hack. We do not want Build to know anything
                # about the interpreter so we can't import it and use isinstance.
                # This should be reliable enough.
                if hasattr(dep, 'subproject'):
                    raise InvalidArguments('Tried to use subproject object as a dependency.\n'
                            'You probably wanted to use a dependency declared in it instead.\n'
                            'Access it by calling get_variable() on the subproject object.')
                raise InvalidArguments('Argument is of an unacceptable type {!r}.\nMust be '
                        'either an external dependency (returned by find_library() or '
                        'dependency()) or an internal dependency (returned by '
                        'declare_dependency()).'.format(type(dep).__name__))

    def get_external_deps(self):
        return self.external_deps

    def link(self, target):
        if not isinstance(target, list):
            target = [target]
        for t in target:
            if hasattr(t, 'held_object'):
                t = t.held_object
            if not isinstance(t, (StaticLibrary, SharedLibrary)):
                raise InvalidArguments('Link target {!r} is not library.'.format(t))
            if isinstance(self, SharedLibrary) and isinstance(t, StaticLibrary) and not t.pic:
                msg = "Can't link non-PIC static library {!r} into shared library {!r}. ".format(t.name, self.name)
                msg += "Use the 'pic' option to static_library to build with PIC."
                raise InvalidArguments(msg)
            if self.is_cross != t.is_cross:
                raise InvalidArguments('Tried to mix cross built and native libraries in target {!r}'.format(self.name))
            self.link_targets.append(t)

    def add_pch(self, language, pchlist):
        if len(pchlist) == 0:
            return
        elif len(pchlist) == 1:
            if not environment.is_header(pchlist[0]):
                raise InvalidArguments('PCH argument %s is not a header.' % pchlist[0])
        elif len(pchlist) == 2:
            if environment.is_header(pchlist[0]):
                if not environment.is_source(pchlist[1]):
                    raise InvalidArguments('PCH definition must contain one header and at most one source.')
            elif environment.is_source(pchlist[0]):
                if not environment.is_header(pchlist[1]):
                    raise InvalidArguments('PCH definition must contain one header and at most one source.')
                pchlist = [pchlist[1], pchlist[0]]
            else:
                raise InvalidArguments('PCH argument %s is of unknown type.' % pchlist[0])
        elif len(pchlist) > 2:
            raise InvalidArguments('PCH definition may have a maximum of 2 files.')
        self.pch[language] = pchlist

    def add_include_dirs(self, args):
        ids = []
        for a in args:
            # FIXME same hack, forcibly unpack from holder.
            if hasattr(a, 'held_object'):
                a = a.held_object
            if not isinstance(a, IncludeDirs):
                raise InvalidArguments('Include directory to be added is not an include directory object.')
            ids.append(a)
        self.include_dirs += ids

    def add_compiler_args(self, language, args):
        args = flatten(args)
        for a in args:
            if not isinstance(a, (str, File)):
                raise InvalidArguments('A non-string passed to compiler args.')
        if language in self.extra_args:
            self.extra_args[language] += args
        else:
            self.extra_args[language] = args

    def get_aliases(self):
        return {}

    def get_clike_dynamic_linker(self):
        '''
        We use the order of languages in `clike_langs` to determine which
        linker to use in case the target has sources compiled with multiple
        compilers. All languages other than those in this list have their own
        linker.
        Note that Vala outputs C code, so Vala sources can use any linker
        that can link compiled C. We don't actually need to add an exception
        for Vala here because of that.
        '''
        for l in clike_langs:
            if l in self.compilers:
                return self.compilers[l]

    def get_using_msvc(self):
        '''
        Check if the dynamic linker is MSVC. Used by Executable, StaticLibrary,
        and SharedLibrary for deciding when to use MSVC-specific file naming
        and debug filenames.

        If at least some code is built with MSVC and the final library is
        linked with MSVC, we can be sure that some debug info will be
        generated. We only check the dynamic linker here because the static
        linker is guaranteed to be of the same type.

        Interesting cases:
        1. The Vala compiler outputs C code to be compiled by whatever
           C compiler we're using, so all objects will still be created by the
           MSVC compiler.
        2. If the target contains only objects, process_compilers guesses and
           picks the first compiler that smells right.
        '''
        linker = self.get_clike_dynamic_linker()
        if linker and linker.get_id() == 'msvc':
            return True
        return False


class Generator():
    def __init__(self, args, kwargs):
        if len(args) != 1:
            raise InvalidArguments('Generator requires exactly one positional argument: the executable')
        exe = args[0]
        if hasattr(exe, 'held_object'):
            exe = exe.held_object
        if not isinstance(exe, (Executable, dependencies.ExternalProgram)):
            raise InvalidArguments('First generator argument must be an executable.')
        self.exe = exe
        self.depfile = None
        self.process_kwargs(kwargs)

    def __repr__(self):
        repr_str = "<{0}: {1}>"
        return repr_str.format(self.__class__.__name__, self.exe)

    def get_exe(self):
        return self.exe

    def process_kwargs(self, kwargs):
        if 'arguments' not in kwargs:
            raise InvalidArguments('Generator must have "arguments" keyword argument.')
        args = kwargs['arguments']
        if isinstance(args, str):
            args = [args]
        if not isinstance(args, list):
            raise InvalidArguments('"Arguments" keyword argument must be a string or a list of strings.')
        for a in args:
            if not isinstance(a, str):
                raise InvalidArguments('A non-string object in "arguments" keyword argument.')
        self.arglist = args
        if 'output' not in kwargs:
            raise InvalidArguments('Generator must have "output" keyword argument.')
        outputs = kwargs['output']
        if not isinstance(outputs, list):
            outputs = [outputs]
        for rule in outputs:
            if not isinstance(rule, str):
                raise InvalidArguments('"output" may only contain strings.')
            if '@BASENAME@' not in rule and '@PLAINNAME@' not in rule:
                raise InvalidArguments('Every element of "output" must contain @BASENAME@ or @PLAINNAME@.')
            if '/' in rule or '\\' in rule:
                raise InvalidArguments('"outputs" must not contain a directory separator.')
        if len(outputs) > 1:
            for o in outputs:
                if '@OUTPUT@' in o:
                    raise InvalidArguments('Tried to use @OUTPUT@ in a rule with more than one output.')
        self.outputs = outputs
        if 'depfile' in kwargs:
            depfile = kwargs['depfile']
            if not isinstance(depfile, str):
                raise InvalidArguments('Depfile must be a string.')
            if os.path.split(depfile)[1] != depfile:
                raise InvalidArguments('Depfile must be a plain filename without a subdirectory.')
            self.depfile = depfile

    def get_base_outnames(self, inname):
        plainname = os.path.split(inname)[1]
        basename = os.path.splitext(plainname)[0]
        return [x.replace('@BASENAME@', basename).replace('@PLAINNAME@', plainname) for x in self.outputs]

    def get_dep_outname(self, inname):
        if self.depfile is None:
            raise InvalidArguments('Tried to get dep name for rule that does not have dependency file defined.')
        plainname = os.path.split(inname)[1]
        basename = os.path.splitext(plainname)[0]
        return self.depfile.replace('@BASENAME@', basename).replace('@PLAINNAME@', plainname)

    def get_arglist(self):
        return self.arglist

    def process_files(self, name, files, state, extra_args=[]):
        output = GeneratedList(self, extra_args=extra_args)
        for f in files:
            if not isinstance(f, str):
                raise InvalidArguments('{} arguments must be strings.'.format(name))
            output.add_file(os.path.join(state.subdir, f))
        return output


class GeneratedList():
    def __init__(self, generator, extra_args=[]):
        if hasattr(generator, 'held_object'):
            generator = generator.held_object
        self.generator = generator
        self.name = self.generator.exe
        self.infilelist = []
        self.outfilelist = []
        self.outmap = {}
        self.extra_depends = []
        self.extra_args = extra_args

    def add_file(self, newfile):
        self.infilelist.append(newfile)
        outfiles = self.generator.get_base_outnames(newfile)
        self.outfilelist += outfiles
        self.outmap[newfile] = outfiles

    def get_inputs(self):
        return self.infilelist

    def get_outputs(self):
        return self.outfilelist

    def get_outputs_for(self, filename):
        return self.outmap[filename]

    def get_generator(self):
        return self.generator

    def get_extra_args(self):
        return self.extra_args

class Executable(BuildTarget):
    def __init__(self, name, subdir, subproject, is_cross, sources, objects, environment, kwargs):
        super().__init__(name, subdir, subproject, is_cross, sources, objects, environment, kwargs)
        # Unless overriden, executables have no suffix or prefix. Except on
        # Windows and with C#/Mono executables where the suffix is 'exe'
        if not hasattr(self, 'prefix'):
            self.prefix = ''
        if not hasattr(self, 'suffix'):
            # Executable for Windows or C#/Mono
            if for_windows(is_cross, environment) or 'cs' in self.compilers:
                self.suffix = 'exe'
            else:
                self.suffix = ''
        self.filename = self.name
        if self.suffix:
            self.filename += '.' + self.suffix
        # See determine_debug_filenames() in build.SharedLibrary
        buildtype = environment.coredata.get_builtin_option('buildtype')
        if self.get_using_msvc() and buildtype.startswith('debug'):
            self.debug_filename = self.prefix + self.name + '.pdb'

    def type_suffix(self):
        return "@exe"

class StaticLibrary(BuildTarget):
    def __init__(self, name, subdir, subproject, is_cross, sources, objects, environment, kwargs):
        if 'pic' not in kwargs and 'b_staticpic' in environment.coredata.base_options:
            kwargs['pic'] = environment.coredata.base_options['b_staticpic'].value
        super().__init__(name, subdir, subproject, is_cross, sources, objects, environment, kwargs)
        if 'cs' in self.compilers:
            raise InvalidArguments('Static libraries not supported for C#.')
        # By default a static library is named libfoo.a even on Windows because
        # MSVC does not have a consistent convention for what static libraries
        # are called. The MSVC CRT uses libfoo.lib syntax but nothing else uses
        # it and GCC only looks for static libraries called foo.lib and
        # libfoo.a. However, we cannot use foo.lib because that's the same as
        # the import library. Using libfoo.a is ok because people using MSVC
        # always pass the library filename while linking anyway.
        if not hasattr(self, 'prefix'):
            self.prefix = 'lib'
        if not hasattr(self, 'suffix'):
            # Rust static library crates have .rlib suffix
            if 'rust' in self.compilers:
                self.suffix = 'rlib'
            else:
                self.suffix = 'a'
        self.filename = self.prefix + self.name + '.' + self.suffix
        # See determine_debug_filenames() in build.SharedLibrary
        buildtype = environment.coredata.get_builtin_option('buildtype')
        if self.get_using_msvc() and buildtype.startswith('debug'):
            self.debug_filename = self.prefix + self.name + '.pdb'

    def type_suffix(self):
        return "@sta"

    def check_unknown_kwargs(self, kwargs):
        self.check_unknown_kwargs_int(kwargs, known_lib_kwargs)

class SharedLibrary(BuildTarget):
    def __init__(self, name, subdir, subproject, is_cross, sources, objects, environment, kwargs):
        self.soversion = None
        self.ltversion = None
        self.vs_module_defs = None
        # The import library this target will generate
        self.import_filename = None
        # The import library that Visual Studio would generate (and accept)
        self.vs_import_filename = None
        # The import library that GCC would generate (and prefer)
        self.gcc_import_filename = None
        super().__init__(name, subdir, subproject, is_cross, sources, objects, environment, kwargs)
        if not hasattr(self, 'prefix'):
            self.prefix = None
        if not hasattr(self, 'suffix'):
            self.suffix = None
        self.basic_filename_tpl = '{0.prefix}{0.name}.{0.suffix}'
        self.determine_filenames(is_cross, environment)
        self.determine_debug_filenames(is_cross, environment)

    def determine_filenames(self, is_cross, env):
        """
        See https://github.com/mesonbuild/meson/pull/417 for details.

        First we determine the filename template (self.filename_tpl), then we
        set the output filename (self.filename).

        The template is needed while creating aliases (self.get_aliases),
        which are needed while generating .so shared libraries for Linux.

        Besides this, there's also the import library name, which is only used
        on Windows since on that platform the linker uses a separate library
        called the "import library" during linking instead of the shared
        library (DLL). The toolchain will output an import library in one of
        two formats: GCC or Visual Studio.

        When we're building with Visual Studio, the import library that will be
        generated by the toolchain is self.vs_import_filename, and with
        MinGW/GCC, it's self.gcc_import_filename. self.import_filename will
        always contain the import library name this target will generate.
        """
        prefix = ''
        suffix = ''
        self.filename_tpl = self.basic_filename_tpl
        # If the user already provided the prefix and suffix to us, we don't
        # need to do any filename suffix/prefix detection.
        # NOTE: manual prefix/suffix override is currently only tested for C/C++
        if self.prefix != None and self.suffix != None:
            pass
        # C# and Mono
        elif 'cs' in self.compilers:
            prefix = ''
            suffix = 'dll'
            self.filename_tpl = '{0.prefix}{0.name}.{0.suffix}'
        # Rust
        elif 'rust' in self.compilers:
            # Currently, we always build --crate-type=rlib
            prefix = 'lib'
            suffix = 'rlib'
            self.filename_tpl = '{0.prefix}{0.name}.{0.suffix}'
        # C, C++, Swift, Vala
        # Only Windows uses a separate import library for linking
        # For all other targets/platforms import_filename stays None
        elif for_windows(is_cross, env):
            suffix = 'dll'
            self.vs_import_filename = '{0}.lib'.format(self.name)
            self.gcc_import_filename = 'lib{0}.dll.a'.format(self.name)
            if self.get_using_msvc():
                # Shared library is of the form foo.dll
                prefix = ''
                # Import library is called foo.lib
                self.import_filename = self.vs_import_filename
            # Assume GCC-compatible naming
            else:
                # Shared library is of the form libfoo.dll
                prefix = 'lib'
                # Import library is called libfoo.dll.a
                self.import_filename = self.gcc_import_filename
            # Shared library has the soversion if it is defined
            if self.soversion:
                self.filename_tpl = '{0.prefix}{0.name}-{0.soversion}.{0.suffix}'
            else:
                self.filename_tpl = '{0.prefix}{0.name}.{0.suffix}'
        elif for_darwin(is_cross, env):
            prefix = 'lib'
            suffix = 'dylib'
            # libfoo.dylib
            self.filename_tpl = '{0.prefix}{0.name}.{0.suffix}'
            # On OS X, the filename should never have the soversion
            # See: https://github.com/mesonbuild/meson/pull/680
        else:
            prefix = 'lib'
            suffix = 'so'
            if self.ltversion:
                # libfoo.so.X[.Y[.Z]] (.Y and .Z are optional)
                self.filename_tpl = '{0.prefix}{0.name}.{0.suffix}.{0.ltversion}'
            elif self.soversion:
                # libfoo.so.X
                self.filename_tpl = '{0.prefix}{0.name}.{0.suffix}.{0.soversion}'
            else:
                # No versioning, libfoo.so
                self.filename_tpl = '{0.prefix}{0.name}.{0.suffix}'
        if self.prefix == None:
            self.prefix = prefix
        if self.suffix == None:
            self.suffix = suffix
        self.filename = self.filename_tpl.format(self)

    def determine_debug_filenames(self, is_cross, env):
        """
        Determine the debug filename(s) using the prefix/name/etc detected in
        determine_filenames() above.
        """
        buildtype = env.coredata.get_builtin_option('buildtype')
        if self.get_using_msvc() and buildtype.startswith('debug'):
            # Currently we only implement separate debug symbol files for MSVC
            # since the toolchain does it for us. Other toolchains embed the
            # debugging symbols in the file itself by default.
            if self.soversion:
                self.debug_filename = '{0.prefix}{0.name}-{0.soversion}.pdb'.format(self)
            else:
                self.debug_filename = '{0.prefix}{0.name}.pdb'.format(self)

    def process_kwargs(self, kwargs, environment):
        super().process_kwargs(kwargs, environment)
        # Shared library version
        if 'version' in kwargs:
            self.ltversion = kwargs['version']
            if not isinstance(self.ltversion, str):
                raise InvalidArguments('Shared library version needs to be a string, not ' + type(self.ltversion).__name__)
            if not re.fullmatch(r'[0-9]+(\.[0-9]+){0,2}', self.ltversion):
                raise InvalidArguments('Invalid Shared library version "{0}". Must be of the form X.Y.Z where all three are numbers. Y and Z are optional.'.format(self.ltversion))
        # Try to extract/deduce the soversion
        if 'soversion' in kwargs:
            self.soversion = kwargs['soversion']
            if isinstance(self.soversion, int):
                self.soversion = str(self.soversion)
            if not isinstance(self.soversion, str):
                raise InvalidArguments('Shared library soversion is not a string or integer.')
        elif self.ltversion:
            # library version is defined, get the soversion from that
            # We replicate what Autotools does here and take the first
            # number of the version by default.
            self.soversion = self.ltversion.split('.')[0]
        # Visual Studio module-definitions file
        if 'vs_module_defs' in kwargs:
            path = kwargs['vs_module_defs']
            if (os.path.isabs(path)):
                self.vs_module_defs = File.from_absolute_file(path)
            else:
                self.vs_module_defs = File.from_source_file(environment.source_dir, self.subdir, path)
            # link_depends can be an absolute path or relative to self.subdir
            self.link_depends.append(path)

    def check_unknown_kwargs(self, kwargs):
        self.check_unknown_kwargs_int(kwargs, known_lib_kwargs)

    def get_import_filename(self):
        """
        The name of the import library that will be outputted by the compiler

        Returns None if there is no import library required for this platform
        """
        return self.import_filename

    def get_import_filenameslist(self):
        if self.import_filename:
            return [self.vs_import_filename, self.gcc_import_filename]
        return []

    def get_all_link_deps(self):
        return [self] + self.get_transitive_link_deps()

    def get_aliases(self):
        """
        If the versioned library name is libfoo.so.0.100.0, aliases are:
        * libfoo.so.0 (soversion) -> libfoo.so.0.100.0
        * libfoo.so (unversioned; for linking) -> libfoo.so.0
        """
        aliases = {}
        # Aliases are only useful with .so libraries. Also if the .so library
        # ends with .so (no versioning), we don't need aliases.
        if self.suffix != 'so' or self.filename.endswith('.so'):
            return {}
        # If ltversion != soversion we create an soversion alias:
        # libfoo.so.0 -> libfoo.so.0.100.0
        if self.ltversion and self.ltversion != self.soversion:
            if not self.soversion:
                # This is done in self.process_kwargs()
                raise AssertionError('BUG: If library version is defined, soversion must have been defined')
            alias_tpl = self.filename_tpl.replace('ltversion', 'soversion')
            ltversion_filename = alias_tpl.format(self)
            aliases[ltversion_filename] = self.filename
        else:
            ltversion_filename = self.filename
        # Unversioned alias: libfoo.so -> libfoo.so.0
        aliases[self.basic_filename_tpl.format(self)] = ltversion_filename
        return aliases

    def type_suffix(self):
        return "@sha"

# A shared library that is meant to be used with dlopen rather than linking
# into something else.
class SharedModule(SharedLibrary):
    def __init__(self, name, subdir, subproject, is_cross, sources, objects, environment, kwargs):
        if 'version' in kwargs:
            raise MesonException('Shared modules must not specify the version kwarg.')
        if 'soversion' in kwargs:
            raise MesonException('Shared modules must not specify the soversion kwarg.')
        super().__init__(name, subdir, subproject, is_cross, sources, objects, environment, kwargs)

class CustomTarget:
    known_kwargs = {'input' : True,
                    'output' : True,
                    'command' : True,
                    'capture' : False,
                    'install' : True,
                    'install_dir' : True,
                    'build_always' : True,
                    'depends' : True,
                    'depend_files' : True,
                    'depfile' : True,
                    }

    def __init__(self, name, subdir, kwargs, absolute_paths=False):
        self.name = name
        self.subdir = subdir
        self.dependencies = []
        self.extra_depends = []
        self.depend_files = [] # Files that this target depends on but are not on the command line.
        self.depfile = None
        self.process_kwargs(kwargs)
        self.extra_files = []
        # Whether to use absolute paths for all files on the commandline
        self.absolute_paths = absolute_paths
        unknowns = []
        for k in kwargs:
            if k not in CustomTarget.known_kwargs:
                unknowns.append(k)
        if len(unknowns) > 0:
            mlog.warning('Unknown keyword arguments in target %s: %s' %
                         (self.name, ', '.join(unknowns)))

    def __repr__(self):
        repr_str = "<{0} {1}: {2}>"
        return repr_str.format(self.__class__.__name__, self.get_id(), self.command)

    def get_id(self):
        return self.name + self.type_suffix()

    def get_target_dependencies(self):
        deps = self.dependencies[:]
        deps += self.extra_depends
        for c in self.sources:
            if hasattr(c, 'held_object'):
                c = c.held_object
            if isinstance(c, (BuildTarget, CustomTarget, GeneratedList)):
                deps.append(c)
        return deps

    def process_kwargs(self, kwargs):
        self.sources = kwargs.get('input', [])
        if not isinstance(self.sources, list):
            self.sources = [self.sources]
        if 'output' not in kwargs:
            raise InvalidArguments('Missing keyword argument "output".')
        self.output = kwargs['output']
        if not isinstance(self.output, list):
            self.output = [self.output]
        for i in self.output:
            if not(isinstance(i, str)):
                raise InvalidArguments('Output argument not a string.')
            if '/' in i:
                raise InvalidArguments('Output must not contain a path segment.')
        self.capture = kwargs.get('capture', False)
        if self.capture and len(self.output) != 1:
            raise InvalidArguments('Capturing can only output to a single file.')
        if 'command' not in kwargs:
            raise InvalidArguments('Missing keyword argument "command".')
        if 'depfile' in kwargs:
            depfile = kwargs['depfile']
            if not isinstance(depfile, str):
                raise InvalidArguments('Depfile must be a string.')
            if os.path.split(depfile)[1] != depfile:
                raise InvalidArguments('Depfile must be a plain filename without a subdirectory.')
            self.depfile = depfile
        cmd = kwargs['command']
        if not(isinstance(cmd, list)):
            cmd = [cmd]
        final_cmd = []
        for i, c in enumerate(cmd):
            if hasattr(c, 'held_object'):
                c = c.held_object
            if isinstance(c, (str, File)):
                final_cmd.append(c)
            elif isinstance(c, dependencies.ExternalProgram):
                if not c.found():
                    raise InvalidArguments('Tried to use not found external program {!r} in a build rule.'.format(c.name))
                final_cmd += c.get_command()
            elif isinstance(c, (BuildTarget, CustomTarget)):
                self.dependencies.append(c)
                final_cmd.append(c)
            elif isinstance(c, list):
                # Hackety hack, only supports one level of flattening. Should really
                # work to arbtrary depth.
                for s in c:
                    if not isinstance(s, str):
                        raise InvalidArguments('Array as argument %d contains a non-string.' % i)
                    final_cmd.append(s)
            else:
                raise InvalidArguments('Argument %s in "command" is invalid.' % i)
        self.command = final_cmd
        if self.capture:
            for c in self.command:
                if isinstance(c, str) and '@OUTPUT@' in c:
                    raise InvalidArguments('@OUTPUT@ is not allowed when capturing output.')
        if 'install' in kwargs:
            self.install = kwargs['install']
            if not isinstance(self.install, bool):
                raise InvalidArguments('"install" must be boolean.')
            if self.install:
                if 'install_dir' not in kwargs:
                    raise InvalidArguments('"install_dir" not specified.')
                self.install_dir = kwargs['install_dir']
                if not(isinstance(self.install_dir, str)):
                    raise InvalidArguments('"install_dir" must be a string.')
        else:
            self.install = False
        self.build_always = kwargs.get('build_always', False)
        if not isinstance(self.build_always, bool):
            raise InvalidArguments('Argument build_always must be a boolean.')
        extra_deps = kwargs.get('depends', [])
        if not isinstance(extra_deps, list):
            extra_deps = [extra_deps]
        for ed in extra_deps:
            while hasattr(ed, 'held_object'):
                ed = ed.held_object
            if not isinstance(ed, (CustomTarget, BuildTarget)):
                raise InvalidArguments('Can only depend on toplevel targets: custom_target or build_target (executable or a library)')
            self.extra_depends.append(ed)
        depend_files = kwargs.get('depend_files', [])
        if not isinstance(depend_files, list):
            depend_files = [depend_files]
        for i in depend_files:
            if isinstance(i, (File, str)):
                self.depend_files.append(i)
            else:
                mlog.debug(i)
                raise InvalidArguments('Unknown type {!r} in depend_files.'.format(type(i).__name__))

    def get_basename(self):
        return self.name

    def get_dependencies(self):
        return self.dependencies

    def should_install(self):
        return self.install

    def get_custom_install_dir(self):
        return self.install_dir

    def get_subdir(self):
        return self.subdir

    def get_outputs(self):
        return self.output

    def get_filename(self):
        return self.output[0]

    def get_sources(self):
        return self.sources

    def get_generated_sources(self):
        return []

    def type_suffix(self):
        return "@cus"

class RunTarget:
    def __init__(self, name, command, args, dependencies, subdir):
        self.name = name
        self.command = command
        self.args = args
        self.dependencies = dependencies
        self.subdir = subdir

    def __repr__(self):
        repr_str = "<{0} {1}: {2}>"
        return repr_str.format(self.__class__.__name__, self.get_id(), self.command)

    def get_id(self):
        return self.name + self.type_suffix()

    def get_basename(self):
        return self.name

    def get_dependencies(self):
        return self.dependencies

    def get_generated_sources(self):
        return []

    def get_sources(self):
        return []

    def get_subdir(self):
        return self.subdir

    def should_install(self):
        return False

    def get_filename(self):
        return self.name

    def type_suffix(self):
        return "@run"

class Jar(BuildTarget):
    def __init__(self, name, subdir, subproject, is_cross, sources, objects, environment, kwargs):
        super().__init__(name, subdir, subproject, is_cross, sources, objects, environment, kwargs);
        for s in self.sources:
            if not s.endswith('.java'):
                raise InvalidArguments('Jar source %s is not a java file.' % s)
        self.filename = self.name + '.jar'
        self.java_args = kwargs.get('java_args', [])

    def get_main_class(self):
        return self.main_class

    def type_suffix(self):
        return "@jar"

    def get_java_args(self):
        return self.java_args

class ConfigureFile():

    def __init__(self, subdir, sourcename, targetname, configuration_data):
        self.subdir = subdir
        self.sourcename = sourcename
        self.targetname = targetname
        self.configuration_data = configuration_data

    def __repr__(self):
        repr_str = "<{0}: {1} -> {2}>"
        src = os.path.join(self.subdir, self.sourcename)
        dst = os.path.join(self.subdir, self.targetname)
        return repr_str.format(self.__class__.__name__, src, dst)

    def get_configuration_data(self):
        return self.configuration_data

    def get_subdir(self):
        return self.subdir

    def get_source_name(self):
        return self.sourcename

    def get_target_name(self):
        return self.targetname

class ConfigurationData():
    def __init__(self):
        super().__init__()
        self.values = {}

    def __repr__(self):
        return repr(self.values)

    def get(self, name):
        return self.values[name]                 # (val, desc)

    def keys(self):
        return self.values.keys()

# A bit poorly named, but this represents plain data files to copy
# during install.
class Data():
    def __init__(self, sources, install_dir):
        self.sources = sources
        self.install_dir = install_dir
        if not isinstance(self.sources, list):
            self.sources = [self.sources]
        for s in self.sources:
            assert(isinstance(s, File))

class RunScript(dict):
    def __init__(self, script, args):
        super().__init__()
        assert(isinstance(script, list))
        assert(isinstance(args, list))
        self['exe'] = script
        self['args'] = args
