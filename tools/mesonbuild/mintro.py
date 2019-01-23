# Copyright 2014-2016 The Meson development team

# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at

#     http://www.apache.org/licenses/LICENSE-2.0

# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""This is a helper script for IDE developers. It allows you to
extract information such as list of targets, files, compiler flags,
tests and so on. All output is in JSON for simple parsing.

Currently only works for the Ninja backend. Others use generated
project files and don't need this info."""

import json
from . import build, coredata as cdata
from . import environment
from . import mesonlib
from . import astinterpreter
from . import mparser
from . import mlog
from . import compilers
from . import optinterpreter
from .interpreterbase import InvalidArguments
from .backend import backends
import sys, os
import pathlib

def get_meson_info_file(info_dir: str):
    return os.path.join(info_dir, 'meson-info.json')

def get_meson_introspection_version():
    return '1.0.0'

def get_meson_introspection_required_version():
    return ['>=1.0', '<2.0']

def get_meson_introspection_types(coredata: cdata.CoreData = None, builddata: build.Build = None, backend: backends.Backend = None):
    if backend and builddata:
        benchmarkdata = backend.create_test_serialisation(builddata.get_benchmarks())
        testdata = backend.create_test_serialisation(builddata.get_tests())
        installdata = backend.create_install_data()
    else:
        benchmarkdata = testdata = installdata = None

    return {
        'benchmarks': {
            'func': lambda: list_benchmarks(benchmarkdata),
            'desc': 'List all benchmarks.',
        },
        'buildoptions': {
            'func': lambda: list_buildoptions(coredata),
            'desc': 'List all build options.',
        },
        'buildsystem_files': {
            'func': lambda: list_buildsystem_files(builddata),
            'desc': 'List files that make up the build system.',
            'key': 'buildsystem-files',
        },
        'dependencies': {
            'func': lambda: list_deps(coredata),
            'desc': 'List external dependencies.',
        },
        'installed': {
            'func': lambda: list_installed(installdata),
            'desc': 'List all installed files and directories.',
        },
        'projectinfo': {
            'func': lambda: list_projinfo(builddata),
            'desc': 'Information about projects.',
        },
        'targets': {
            'func': lambda: list_targets(builddata, installdata, backend),
            'desc': 'List top level targets.',
        },
        'tests': {
            'func': lambda: list_tests(testdata),
            'desc': 'List all unit tests.',
        }
    }

def add_arguments(parser):
    intro_types = get_meson_introspection_types()
    for key, val in intro_types.items():
        flag = '--' + val.get('key', key)
        parser.add_argument(flag, action='store_true', dest=key, default=False, help=val['desc'])

    parser.add_argument('--target-files', action='store', dest='target_files', default=None,
                        help='List source files for a given target.')
    parser.add_argument('--backend', choices=cdata.backendlist, dest='backend', default='ninja',
                        help='The backend to use for the --buildoptions introspection.')
    parser.add_argument('-a', '--all', action='store_true', dest='all', default=False,
                        help='Print all available information.')
    parser.add_argument('-i', '--indent', action='store_true', dest='indent', default=False,
                        help='Enable pretty printed JSON.')
    parser.add_argument('-f', '--force-object-output', action='store_true', dest='force_dict', default=False,
                        help='Always use the new JSON format for multiple entries (even for 0 and 1 introspection commands)')
    parser.add_argument('builddir', nargs='?', default='.', help='The build directory')

def list_installed(installdata):
    res = {}
    if installdata is not None:
        for t in installdata.targets:
            res[os.path.join(installdata.build_dir, t.fname)] = \
                os.path.join(installdata.prefix, t.outdir, os.path.basename(t.fname))
        for path, installpath, unused_prefix in installdata.data:
            res[path] = os.path.join(installdata.prefix, installpath)
        for path, installdir, unused_custom_install_mode in installdata.headers:
            res[path] = os.path.join(installdata.prefix, installdir, os.path.basename(path))
        for path, installpath, unused_custom_install_mode in installdata.man:
            res[path] = os.path.join(installdata.prefix, installpath)
    return res

def list_targets(builddata: build.Build, installdata, backend: backends.Backend):
    tlist = []
    build_dir = builddata.environment.get_build_dir()
    src_dir = builddata.environment.get_source_dir()

    # Fast lookup table for installation files
    install_lookuptable = {}
    for i in installdata.targets:
        outname = os.path.join(installdata.prefix, i.outdir, os.path.basename(i.fname))
        install_lookuptable[os.path.basename(i.fname)] = str(pathlib.PurePath(outname))

    for (idname, target) in builddata.get_targets().items():
        if not isinstance(target, build.Target):
            raise RuntimeError('The target object in `builddata.get_targets()` is not of type `build.Target`. Please file a bug with this error message.')

        t = {
            'name': target.get_basename(),
            'id': idname,
            'type': target.get_typename(),
            'defined_in': os.path.normpath(os.path.join(src_dir, target.subdir, 'meson.build')),
            'filename': [os.path.join(build_dir, target.subdir, x) for x in target.get_outputs()],
            'build_by_default': target.build_by_default,
            'target_sources': backend.get_introspection_data(idname, target)
        }

        if installdata and target.should_install():
            t['installed'] = True
            t['install_filename'] = [install_lookuptable.get(x, None) for x in target.get_outputs()]
        else:
            t['installed'] = False
        tlist.append(t)
    return tlist

class IntrospectionHelper:
    # mimic an argparse namespace
    def __init__(self, cross_file):
        self.cross_file = cross_file
        self.native_file = None
        self.cmd_line_options = {}

class IntrospectionInterpreter(astinterpreter.AstInterpreter):
    # Interpreter to detect the options without a build directory
    # Most of the code is stolen from interperter.Interpreter
    def __init__(self, source_root, subdir, backend, cross_file=None, subproject='', subproject_dir='subprojects', env=None):
        super().__init__(source_root, subdir)

        options = IntrospectionHelper(cross_file)
        self.cross_file = cross_file
        if env is None:
            self.environment = environment.Environment(source_root, None, options)
        else:
            self.environment = env
        self.subproject = subproject
        self.subproject_dir = subproject_dir
        self.coredata = self.environment.get_coredata()
        self.option_file = os.path.join(self.source_root, self.subdir, 'meson_options.txt')
        self.backend = backend
        self.default_options = {'backend': self.backend}
        self.project_data = {}

        self.funcs.update({
            'project': self.func_project,
            'add_languages': self.func_add_languages
        })

    def flatten_args(self, args):
        # Resolve mparser.ArrayNode if needed
        flattend_args = []
        if isinstance(args, mparser.ArrayNode):
            args = [x.value for x in args.args.arguments]
        for i in args:
            if isinstance(i, mparser.ArrayNode):
                flattend_args += [x.value for x in i.args.arguments]
            elif isinstance(i, str):
                flattend_args += [i]
            else:
                pass
        return flattend_args

    def func_project(self, node, args, kwargs):
        if len(args) < 1:
            raise InvalidArguments('Not enough arguments to project(). Needs at least the project name.')

        proj_name = args[0]
        proj_vers = kwargs.get('version', 'undefined')
        proj_langs = self.flatten_args(args[1:])
        if isinstance(proj_vers, mparser.ElementaryNode):
            proj_vers = proj_vers.value
        if not isinstance(proj_vers, str):
            proj_vers = 'undefined'
        self.project_data = {'descriptive_name': proj_name, 'version': proj_vers}

        if os.path.exists(self.option_file):
            oi = optinterpreter.OptionInterpreter(self.subproject)
            oi.process(self.option_file)
            self.coredata.merge_user_options(oi.options)

        def_opts = self.flatten_args(kwargs.get('default_options', []))
        self.project_default_options = mesonlib.stringlistify(def_opts)
        self.project_default_options = cdata.create_options_dict(self.project_default_options)
        self.default_options.update(self.project_default_options)
        self.coredata.set_default_options(self.default_options, self.subproject, self.environment.cmd_line_options)

        if not self.is_subproject() and 'subproject_dir' in kwargs:
            spdirname = kwargs['subproject_dir']
            if isinstance(spdirname, str):
                self.subproject_dir = spdirname
        if not self.is_subproject():
            self.project_data['subprojects'] = []
            subprojects_dir = os.path.join(self.source_root, self.subproject_dir)
            if os.path.isdir(subprojects_dir):
                for i in os.listdir(subprojects_dir):
                    if os.path.isdir(os.path.join(subprojects_dir, i)):
                        self.do_subproject(i)

        self.coredata.init_backend_options(self.backend)
        options = {k: v for k, v in self.environment.cmd_line_options.items() if k.startswith('backend_')}

        self.coredata.set_options(options)
        self.func_add_languages(None, proj_langs, None)

    def do_subproject(self, dirname):
        subproject_dir_abs = os.path.join(self.environment.get_source_dir(), self.subproject_dir)
        subpr = os.path.join(subproject_dir_abs, dirname)
        try:
            subi = IntrospectionInterpreter(subpr, '', self.backend, cross_file=self.cross_file, subproject=dirname, subproject_dir=self.subproject_dir, env=self.environment)
            subi.analyze()
            subi.project_data['name'] = dirname
            self.project_data['subprojects'] += [subi.project_data]
        except:
            return

    def func_add_languages(self, node, args, kwargs):
        args = self.flatten_args(args)
        need_cross_compiler = self.environment.is_cross_build()
        for lang in sorted(args, key=compilers.sort_clink):
            lang = lang.lower()
            if lang not in self.coredata.compilers:
                self.environment.detect_compilers(lang, need_cross_compiler)

    def is_subproject(self):
        return self.subproject != ''

    def analyze(self):
        self.load_root_meson_file()
        self.sanity_check_ast()
        self.parse_project()
        self.run()

def list_buildoptions_from_source(sourcedir, backend, indent):
    # Make sure that log entries in other parts of meson don't interfere with the JSON output
    mlog.disable()
    backend = backends.get_backend_from_name(backend, None)
    intr = IntrospectionInterpreter(sourcedir, '', backend.name)
    intr.analyze()
    # Reenable logging just in case
    mlog.enable()
    print(json.dumps(list_buildoptions(intr.coredata), indent=indent))

def list_target_files(target_name: str, targets: list, source_dir: str):
    sys.stderr.write("WARNING: The --target-files introspection API is deprecated. Use --targets instead.\n")
    result = []
    tgt = None

    for i in targets:
        if i['id'] == target_name:
            tgt = i
            break

    if tgt is None:
        print('Target with the ID "{}" could not be found'.format(target_name))
        sys.exit(1)

    for i in tgt['target_sources']:
        result += i['sources'] + i['generated_sources']

    result = list(map(lambda x: os.path.relpath(x, source_dir), result))

    return result

def list_buildoptions(coredata: cdata.CoreData):
    optlist = []

    dir_option_names = ['bindir',
                        'datadir',
                        'includedir',
                        'infodir',
                        'libdir',
                        'libexecdir',
                        'localedir',
                        'localstatedir',
                        'mandir',
                        'prefix',
                        'sbindir',
                        'sharedstatedir',
                        'sysconfdir']
    test_option_names = ['errorlogs',
                         'stdsplit']
    core_option_names = [k for k in coredata.builtins if k not in dir_option_names + test_option_names]

    dir_options = {k: o for k, o in coredata.builtins.items() if k in dir_option_names}
    test_options = {k: o for k, o in coredata.builtins.items() if k in test_option_names}
    core_options = {k: o for k, o in coredata.builtins.items() if k in core_option_names}

    add_keys(optlist, core_options, 'core')
    add_keys(optlist, coredata.backend_options, 'backend')
    add_keys(optlist, coredata.base_options, 'base')
    add_keys(optlist, coredata.compiler_options, 'compiler')
    add_keys(optlist, dir_options, 'directory')
    add_keys(optlist, coredata.user_options, 'user')
    add_keys(optlist, test_options, 'test')
    return optlist

def add_keys(optlist, options, section):
    keys = list(options.keys())
    keys.sort()
    for key in keys:
        opt = options[key]
        optdict = {'name': key, 'value': opt.value, 'section': section}
        if isinstance(opt, cdata.UserStringOption):
            typestr = 'string'
        elif isinstance(opt, cdata.UserBooleanOption):
            typestr = 'boolean'
        elif isinstance(opt, cdata.UserComboOption):
            optdict['choices'] = opt.choices
            typestr = 'combo'
        elif isinstance(opt, cdata.UserIntegerOption):
            typestr = 'integer'
        elif isinstance(opt, cdata.UserArrayOption):
            typestr = 'array'
        else:
            raise RuntimeError("Unknown option type")
        optdict['type'] = typestr
        optdict['description'] = opt.description
        optlist.append(optdict)

def find_buildsystem_files_list(src_dir):
    # I feel dirty about this. But only slightly.
    filelist = []
    for root, _, files in os.walk(src_dir):
        for f in files:
            if f == 'meson.build' or f == 'meson_options.txt':
                filelist.append(os.path.relpath(os.path.join(root, f), src_dir))
    return filelist

def list_buildsystem_files(builddata: build.Build):
    src_dir = builddata.environment.get_source_dir()
    filelist = find_buildsystem_files_list(src_dir)
    filelist = [os.path.join(src_dir, x) for x in filelist]
    return filelist

def list_deps(coredata: cdata.CoreData):
    result = []
    for d in coredata.deps.values():
        if d.found():
            result += [{'name': d.name,
                        'compile_args': d.get_compile_args(),
                        'link_args': d.get_link_args()}]
    return result

def get_test_list(testdata):
    result = []
    for t in testdata:
        to = {}
        if isinstance(t.fname, str):
            fname = [t.fname]
        else:
            fname = t.fname
        to['cmd'] = fname + t.cmd_args
        if isinstance(t.env, build.EnvironmentVariables):
            to['env'] = t.env.get_env(os.environ)
        else:
            to['env'] = t.env
        to['name'] = t.name
        to['workdir'] = t.workdir
        to['timeout'] = t.timeout
        to['suite'] = t.suite
        to['is_parallel'] = t.is_parallel
        result.append(to)
    return result

def list_tests(testdata):
    return get_test_list(testdata)

def list_benchmarks(benchdata):
    return get_test_list(benchdata)

def list_projinfo(builddata: build.Build):
    result = {'version': builddata.project_version,
              'descriptive_name': builddata.project_name}
    subprojects = []
    for k, v in builddata.subprojects.items():
        c = {'name': k,
             'version': v,
             'descriptive_name': builddata.projects.get(k)}
        subprojects.append(c)
    result['subprojects'] = subprojects
    return result

def list_projinfo_from_source(sourcedir, indent):
    files = find_buildsystem_files_list(sourcedir)
    files = [os.path.normpath(x) for x in files]

    mlog.disable()
    intr = IntrospectionInterpreter(sourcedir, '', 'ninja')
    intr.analyze()
    mlog.enable()

    for i in intr.project_data['subprojects']:
        basedir = os.path.join(intr.subproject_dir, i['name'])
        i['buildsystem_files'] = [x for x in files if x.startswith(basedir)]
        files = [x for x in files if not x.startswith(basedir)]

    intr.project_data['buildsystem_files'] = files
    print(json.dumps(intr.project_data, indent=indent))

def run(options):
    datadir = 'meson-private'
    infodir = 'meson-info'
    indent = 4 if options.indent else None
    if options.builddir is not None:
        datadir = os.path.join(options.builddir, datadir)
        infodir = os.path.join(options.builddir, infodir)
    if options.builddir.endswith('/meson.build') or options.builddir.endswith('\\meson.build') or options.builddir == 'meson.build':
        sourcedir = '.' if options.builddir == 'meson.build' else options.builddir[:-11]
        if options.projectinfo:
            list_projinfo_from_source(sourcedir, indent)
            return 0
        if options.buildoptions:
            list_buildoptions_from_source(sourcedir, options.backend, indent)
            return 0
    infofile = get_meson_info_file(infodir)
    if not os.path.isdir(datadir) or not os.path.isdir(infodir) or not os.path.isfile(infofile):
        print('Current directory is not a meson build directory.'
              'Please specify a valid build dir or change the working directory to it.'
              'It is also possible that the build directory was generated with an old'
              'meson version. Please regenerate it in this case.')
        return 1

    intro_vers = '0.0.0'
    source_dir = None
    with open(infofile, 'r') as fp:
        raw = json.load(fp)
        intro_vers = raw.get('introspection', {}).get('version', {}).get('full', '0.0.0')
        source_dir = raw.get('directories', {}).get('source', None)

    vers_to_check = get_meson_introspection_required_version()
    for i in vers_to_check:
        if not mesonlib.version_compare(intro_vers, i):
            print('Introspection version {} is not supported. '
                  'The required version is: {}'
                  .format(intro_vers, ' and '.join(vers_to_check)))
            return 1

    results = []
    intro_types = get_meson_introspection_types()

    # Handle the one option that does not have its own JSON file (meybe deprecate / remove this?)
    if options.target_files is not None:
        targets_file = os.path.join(infodir, 'intro-targets.json')
        with open(targets_file, 'r') as fp:
            targets = json.load(fp)
        results += [('target_files', list_target_files(options.target_files, targets, source_dir))]

    # Extract introspection information from JSON
    for i in intro_types.keys():
        if not options.all and not getattr(options, i, False):
            continue
        curr = os.path.join(infodir, 'intro-{}.json'.format(i))
        if not os.path.isfile(curr):
            print('Introspection file {} does not exist.'.format(curr))
            return 1
        with open(curr, 'r') as fp:
            results += [(i, json.load(fp))]

    if len(results) == 0 and not options.force_dict:
        print('No command specified')
        return 1
    elif len(results) == 1 and not options.force_dict:
        # Make to keep the existing output format for a single option
        print(json.dumps(results[0][1], indent=indent))
    else:
        out = {}
        for i in results:
            out[i[0]] = i[1]
        print(json.dumps(out, indent=indent))
    return 0

updated_introspection_files = []

def write_intro_info(intro_info, info_dir):
    global updated_introspection_files
    for i in intro_info:
        out_file = os.path.join(info_dir, 'intro-{}.json'.format(i[0]))
        tmp_file = os.path.join(info_dir, 'tmp_dump.json')
        with open(tmp_file, 'w') as fp:
            json.dump(i[1], fp)
            fp.flush() # Not sure if this is needed
        os.replace(tmp_file, out_file)
        updated_introspection_files += [i[0]]

def generate_introspection_file(builddata: build.Build, backend: backends.Backend):
    coredata = builddata.environment.get_coredata()
    intro_types = get_meson_introspection_types(coredata=coredata, builddata=builddata, backend=backend)
    intro_info = []

    for key, val in intro_types.items():
        intro_info += [(key, val['func']())]

    write_intro_info(intro_info, builddata.environment.info_dir)

def update_build_options(coredata: cdata.CoreData, info_dir):
    intro_info = [
        ('buildoptions', list_buildoptions(coredata))
    ]

    write_intro_info(intro_info, info_dir)

def split_version_string(version: str):
    vers_list = version.split('.')
    return {
        'full': version,
        'major': int(vers_list[0] if len(vers_list) > 0 else 0),
        'minor': int(vers_list[1] if len(vers_list) > 1 else 0),
        'patch': int(vers_list[2] if len(vers_list) > 2 else 0)
    }

def write_meson_info_file(builddata: build.Build, errors: list, build_files_updated: bool = False):
    global updated_introspection_files
    info_dir = builddata.environment.info_dir
    info_file = get_meson_info_file(info_dir)
    intro_types = get_meson_introspection_types()
    intro_info = {}

    for i in intro_types.keys():
        intro_info[i] = {
            'file': 'intro-{}.json'.format(i),
            'updated': i in updated_introspection_files
        }

    info_data = {
        'meson_version': split_version_string(cdata.version),
        'directories': {
            'source': builddata.environment.get_source_dir(),
            'build': builddata.environment.get_build_dir(),
            'info': info_dir,
        },
        'introspection': {
            'version': split_version_string(get_meson_introspection_version()),
            'information': intro_info,
        },
        'build_files_updated': build_files_updated,
    }

    if len(errors) > 0:
        info_data['error'] = True
        info_data['error_list'] = [x if isinstance(x, str) else str(x) for x in errors]
    else:
        info_data['error'] = False

    # Write the data to disc
    tmp_file = os.path.join(info_dir, 'tmp_dump.json')
    with open(tmp_file, 'w') as fp:
        json.dump(info_data, fp)
        fp.flush()
    os.replace(tmp_file, info_file)
