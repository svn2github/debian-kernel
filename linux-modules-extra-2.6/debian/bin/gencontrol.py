#!/usr/bin/env python2.4
import sys
sys.path.append(sys.argv[1] + "/lib/python")
import debian_linux.gencontrol
from debian_linux import config
from debian_linux.debian import *

class gencontrol(debian_linux.gencontrol.gencontrol):
    def __init__(self, configdir):
        super(gencontrol, self).__init__(configdir)
        self.process_changelog()
        self.config = config_reader_modules(self.config)

    def do_main_setup(self, vars, makeflags, extra):
        super(gencontrol, self).do_main_setup(vars, makeflags, extra)
        makeflags.update({
            'SOURCEVERSION': self.version['linux']['source'],
        })

    def do_main_makefile(self, makefile, makeflags, extra):
        makefile.append(("binary-indep:", []))

    def do_main_packages(self, packages, extra):
        vars = self.vars

        packages['source']['Build-Depends'].extend(
            ['linux-support-%s%s' % (self.version['linux']['upstream'], self.abiname)]
        )
        packages['source']['Build-Depends'].extend(
            ['linux-headers-%s%s-all-%s [%s]' % (self.version['linux']['upstream'], self.abiname, arch, arch)
            for arch in self.config['base',]['arches']],
        )

        for module in iter(self.config['base',]['modules']):
            packages['source']['Build-Depends'].append('%s-source' % module)

    def do_flavour(self, packages, makefile, arch, subarch, flavour, vars, makeflags, extra):
        config_entry = self.config.merge('base', None, arch, subarch, flavour)
        if config_entry.get('modules', True) is False:
            return

        super(gencontrol, self).do_flavour(packages, makefile, arch, subarch, flavour, vars, makeflags, extra)

        for module in iter(self.config['base',]['modules']):
            self.do_module(module, packages, makefile, arch, subarch, flavour, vars.copy(), makeflags.copy(), extra)

    def do_flavour_makefile(self, makefile, arch, subarch, flavour, makeflags, extra):
        for i in self.makefile_targets:
            makefile.append("%s-%s-%s:: %s-%s-%s-%s" % (i, arch, subarch, i, arch, subarch, flavour))

    def do_module(self, module, packages, makefile, arch, subarch, flavour, vars, makeflags, extra):
        config_entry = self.config['base', module]
        vars.update(config_entry)
        vars['module'] = module
        makeflags['MODULE'] = module
        makeflags['MODULESOURCE'] = "%s-source" % module

        if not vars.get('longdesc', None):
            vars['longdesc'] = ''

        if arch not in config_entry.get('arches', [arch]):
            return
        if arch in config_entry.get('not-arches', []):
            return
        if subarch not in config_entry.get('subarches', [subarch]):
            return
        if subarch in config_entry.get('not-subarches', []):
            return

        modules = self.templates["control.modules"]
        modules = self.process_packages(modules, vars)

        for package in modules:
            name = package['Package']
            if packages.has_key(name):
                package = packages.get(name)
                package['Architecture'].append(arch)
            else:
                package['Architecture'] = [arch]
                packages.append(package)

        makeflags_string = ' '.join(["%s='%s'" % i for i in makeflags.iteritems()])

        cmds_binary_arch = []
        cmds_binary_arch.append(("$(MAKE) -f debian/rules.real binary-arch %s" % makeflags_string,))
        cmds_build = []
        cmds_build.append(("$(MAKE) -f debian/rules.real build %s" % makeflags_string,))
        cmds_setup = []
        cmds_setup.append(("$(MAKE) -f debian/rules.real setup %s" % makeflags_string,))
        for i in self.makefile_targets:
            makefile.append("%s-%s-%s-%s:: %s-%s-%s-%s-%s" % (i, arch, subarch, flavour, i, arch, subarch, flavour, module))
        makefile.append(("binary-arch-%s-%s-%s-%s:" % (arch, subarch, flavour, module), cmds_binary_arch))
        makefile.append(("build-%s-%s-%s-%s:" % (arch, subarch, flavour, module), cmds_build))
        makefile.append(("setup-%s-%s-%s-%s:" % (arch, subarch, flavour, module), cmds_setup))

    def process_changelog(self):
        changelog = read_changelog()
        self.version = changelog[0]['Version']
        if self.version['linux']['modifier'] is not None:
            self.abiname = ''
        else:
            self.abiname = '-%s' % self.config['abi',]['abiname']
        self.vars = self.process_version_linux(self.version, self.abiname)

class config_reader_modules(config.config_reader):
    schema_base = {
        'modules': config.schema_item_list(),
    }

    schema_module = {
        'arches': config.schema_item_list(),
    }

    def __init__(self, arch):
        super(config_reader_modules, self).__init__(['.'])
        self._read_base()

        for section in iter(arch):
            s1 = self.get(section, {})
            s2 = arch[section].copy()
            s2.update(s1)
            self[section] = s2

    def _read_base(self):
        config_file = config.config_parser(self.schema_base, self._get_files(self.config_name))

        modules = config_file['base',]['modules']

        for section in iter(config_file):
            real = list(section)
            if real[-1] in modules:
                real.insert(0, 'base')
            else:
                real.insert(0, real.pop())
            self[tuple(real)] = config_file[section]

        for module in modules:
            self._read_module(module)

    def _read_module(self, module):
        config_file = config.config_parser(self.schema_base, self._get_files("%s/%s" % (module, self.config_name)))

        for section in iter(config_file):
            real = list(section)
            real[0:] = [real.pop(), module]
            real = tuple(real)
            s = self.get(real, {})
            s.update(config_file[section])
            self[real] = s

    def merge(self, section, module, *args):
        ret = {}
        ret.update(super(config_reader_modules, self).merge(section, *args))
        if module:
            ret.update(super(config_reader_modules, self).merge(section, module, *args))
        return ret

if __name__ == '__main__':
    gencontrol(sys.argv[1] + "/arch")()
