#!/usr/bin/env python

import sys
sys.path.append(sys.argv[1] + "/lib/python")

from debian_linux.config import ConfigCoreDump, ConfigParser, SchemaItemList
from debian_linux.debian import *
from debian_linux.gencontrol import Gencontrol as Base
from debian_linux.utils import Templates

class Gencontrol(Base):
    def __init__(self, config):
        super(Gencontrol, self).__init__(Config(config), Templates(["debian/templates"]))
        self.process_changelog()

    def do_main_setup(self, vars, makeflags, extra):
        super(Gencontrol, self).do_main_setup(vars, makeflags, extra)
        makeflags.update({
            'VERSION_SOURCE': self.package_version.upstream,
            'VERSION_REVISION': self.package_version.revision,
            'UPSTREAMVERSION': self.version.linux_upstream,
            'ABINAME': self.abiname,
        })

    def do_main_makefile(self, makefile, makeflags, extra):
        makefile.add("binary-indep")

    def do_main_packages(self, packages, extra):
        vars = self.vars

        packages['source']['Build-Depends'].extend(
            ['linux-support-%s%s' % (self.version.linux_upstream, self.abiname)]
        )
        packages['source']['Build-Depends'].extend(
            ['linux-headers-%s%s-all-%s [%s]' % (self.version.linux_upstream, self.abiname, arch, arch)
            for arch in self.config['base',]['arches']],
        )

    def do_flavour(self, packages, makefile, arch, featureset, flavour, vars, makeflags, extra):
        config_entry = self.config['module', 'base']

        config_base = self.config.merge('base', arch, featureset, flavour)
        if not config_base.get('modules', True):
            return

        super(Gencontrol, self).do_flavour(packages, makefile, arch, featureset, flavour, vars, makeflags, extra)

        for module in iter(config_entry['modules']):
            self.do_module(module, packages, makefile, arch, featureset, flavour, vars.copy(), makeflags.copy(), extra)

    def do_module(self, module, packages, makefile, arch, featureset, flavour, vars, makeflags, extra):
        config_entry = self.config['module', 'base', module]
        config_entry_relations = self.config.get(('relations', module), {})
        vars.update(config_entry)
        vars['module'] = module
        makeflags['MODULE'] = module

        if not vars.get('longdesc', None):
            vars['longdesc'] = ''

        if arch not in config_entry.get('arches', [arch]):
            return
        if arch in config_entry.get('not-arches', []):
            return
        if featureset not in config_entry.get('featuresets', [featureset]):
            return
        if featureset in config_entry.get('not-featuresets', []):
            return
        if flavour not in config_entry.get('flavours', [flavour]):
            return
        if flavour in config_entry.get('not-flavours', []):
            return

        relations = PackageRelation(config_entry_relations.get('source', '%s-source' % module))
        if config_entry.get('arches', None) or config_entry.get('not-arches', None):
            for group in relations:
                for item in group:
                    item.arches = [arch]
        makeflags['MODULESOURCE'] = relations[0][0].name

        packages['source']['Build-Depends'].extend(relations)

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

        for i in self.makefile_targets:
            target1 = '_'.join((i, arch, featureset, flavour))
            target2 = '_'.join((target1, module))
            makefile.add(target1, [target2])

        cmds_binary_arch = ["$(MAKE) -f debian/rules.real binary-arch %s" % makeflags]
        cmds_build = ["$(MAKE) -f debian/rules.real build %s" % makeflags]
        cmds_setup = ["$(MAKE) -f debian/rules.real setup %s" % makeflags]
        makefile.add("binary-arch_%s_%s_%s_%s" % (arch, featureset, flavour, module), cmds = cmds_binary_arch)
        makefile.add("build_%s_%s_%s_%s" % (arch, featureset, flavour, module), cmds = cmds_build)
        makefile.add("setup_%s_%s_%s_%s" % (arch, featureset, flavour, module), cmds = cmds_setup)

    def process_changelog(self):
        self.package_version = self.changelog[0].version
        self.version = VersionLinux(self.config['version',]['source'])
        self.abiname = self.config['version',]['abiname']
        self.vars = {
            'upstreamversion': self.version.linux_upstream,
            'version': self.version.linux_version,
            'source_upstream': self.version.upstream,
            'major': self.version.linux_major,
            'abiname': self.abiname,
        }

class Config(ConfigCoreDump):
    config_name = "defines"

    schemas_base = {
        'base': {
            'modules': SchemaItemList(),
        }
    }

    schemas_module = {
        'base': {
            'arches': SchemaItemList(),
            'flavours':  SchemaItemList(),
            'not-arches': SchemaItemList(),
            'not-flavours':  SchemaItemList(),
            'not-featuresets': SchemaItemList(),
            'featuresets': SchemaItemList(),
        }
    }

    def __init__(self, config):
        super(Config, self).__init__(fp = file(config))

        self._read_base()

    def _read_base(self):
        config = ConfigParser(self.schemas_base)
        config.read(self.config_name)

        for section in iter(config):
            real = ('module', section[-1],) + section[1:]
            self[real] = config[section]

        for module in config['base',]['modules']:
            self._read_module(module)

    def _read_module(self, module):
        config = ConfigParser(self.schemas_module)
        config.read("%s/%s" % (module, self.config_name))

        for section in iter(config):
            real = ('module', section[-1], module) + section[1:]
            s = self.get(real, {})
            s.update(config[section])
            self[real] = s

if __name__ == '__main__':
    Gencontrol(sys.argv[1] + "/config.defines.dump")()
