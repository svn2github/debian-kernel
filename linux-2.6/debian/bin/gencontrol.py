#!/usr/bin/env python

import os, sys
sys.path.append("debian/lib/python")

from debian_linux.config import ConfigCoreHierarchy
from debian_linux.debian import *
from debian_linux.gencontrol import Gencontrol as Base
from debian_linux.utils import Templates

class Gencontrol(Base):
    def __init__(self, config_dirs = ["debian/config"], template_dirs = ["debian/templates"]):
        super(Gencontrol, self).__init__(ConfigCoreHierarchy(config_dirs), Templates(template_dirs), VersionLinux)
        self.process_changelog()
        self.config_dirs = config_dirs

    def do_main_setup(self, vars, makeflags, extra):
        super(Gencontrol, self).do_main_setup(vars, makeflags, extra)
        vars.update(self.config['image',])
        makeflags.update({
            'SOURCEVERSION': self.version.complete,
        })

    def do_main_packages(self, packages, extra):
        packages.extend(self.process_packages(self.templates["control.main"], self.vars))
        packages.append(self.process_real_tree(self.templates["control.tree"][0], self.vars))
        packages.extend(self.process_packages(self.templates["control.support"], self.vars))

    def do_arch_setup(self, vars, makeflags, arch, extra):
        config_base = self.config.get(('base', arch), {})
        vars.update(self.config.get(('image', arch), {}))
        config_libc_dev = self.config.get(('libc-dev', arch), {})
        arch = config_libc_dev.get('arch', None)
        if arch is None:
            arch = config_base.get('kernel-arch')
        makeflags['LIBC_DEV_ARCH'] = arch

    def do_arch_packages(self, packages, makefile, arch, vars, makeflags, extra):
        headers_arch = self.templates["control.headers.arch"]
        packages_headers_arch = self.process_packages(headers_arch, vars)

        libc_dev = self.templates["control.libc-dev"]
        packages_headers_arch[0:0] = self.process_packages(libc_dev, {})
        
        extra['headers_arch_depends'] = packages_headers_arch[-1]['Depends'] = PackageRelation()

        for package in packages_headers_arch:
            name = package['Package']
            if packages.has_key(name):
                package = packages.get(name)
                package['Architecture'].append(arch)
            else:
                package['Architecture'] = [arch]
                packages.append(package)

        cmds_binary_arch = ["$(MAKE) -f debian/rules.real binary-arch-arch %s" % makeflags]
        cmds_source = ["$(MAKE) -f debian/rules.real source-arch %s" % makeflags]
        makefile.add('binary-arch_%s_real' % arch, cmds = cmds_binary_arch)
        makefile.add('source_%s_real' % arch, cmds = cmds_source)

    def do_featureset_setup(self, vars, makeflags, arch, featureset, extra):
        vars.update(self.config.get(('image', arch, featureset), {}))
        vars['localversion_headers'] = vars['localversion']
        for i in (
            ('kernel-header-dirs', 'KERNEL_HEADER_DIRS'),
            ('localversion_headers', 'LOCALVERSION_HEADERS'),
        ):
            if vars.has_key(i[0]):
                makeflags[i[1]] = vars[i[0]]

    def do_featureset_packages(self, packages, makefile, arch, featureset, vars, makeflags, extra):
        headers_featureset = self.templates["control.headers.featureset"]
        package_headers = self.process_package(headers_featureset[0], vars)

        name = package_headers['Package']
        if packages.has_key(name):
            package_headers = packages.get(name)
            package_headers['Architecture'].append(arch)
        else:
            package_headers['Architecture'] = [arch]
            packages.append(package_headers)

        cmds_binary_arch = ["$(MAKE) -f debian/rules.real binary-arch-featureset %s" % makeflags]
        cmds_source = ["$(MAKE) -f debian/rules.real source-featureset %s" % makeflags]
        makefile.add('binary-arch_%s_%s_real' % (arch, featureset), cmds = cmds_binary_arch)
        makefile.add('source_%s_%s_real' % (arch, featureset), cmds = cmds_source)

    def do_flavour_setup(self, vars, makeflags, arch, featureset, flavour, extra):
        vars.update(self.config.get(('image', arch, featureset, flavour), {}))
        for i in (
            ('cflags', 'CFLAGS'),
            ('compiler', 'COMPILER'),
            ('initramfs', 'INITRAMFS',),
            ('kernel-arch', 'KERNEL_ARCH'),
            ('kernel-header-dirs', 'KERNEL_HEADER_DIRS'),
            ('kpkg-arch', 'KPKG_ARCH'),
            ('kpkg-subarch', 'KPKG_SUBARCH'),
            ('localversion', 'LOCALVERSION'),
            ('override-host-type', 'OVERRIDE_HOST_TYPE'),
            ('type', 'TYPE'),
        ):
            if vars.has_key(i[0]):
                makeflags[i[1]] = vars[i[0]]

    def do_flavour_packages(self, packages, makefile, arch, featureset, flavour, vars, makeflags, extra):
        headers = self.templates["control.headers"]

        config_entry_base = self.config.merge('base', arch, featureset, flavour)
        config_entry_relations = self.config.merge('relations', arch, featureset, flavour)

        compiler = config_entry_base.get('compiler', 'gcc')
        relations_compiler = PackageRelation(config_entry_relations[compiler])
        relations_compiler_build_dep = PackageRelation(config_entry_relations[compiler])
        for group in relations_compiler_build_dep:
            for item in group:
                item.arches = [arch]
        packages['source']['Build-Depends'].extend(relations_compiler_build_dep)

        image_relations = {
            'conflicts': PackageRelation(),
            'depends': PackageRelation(),
        }
        if vars.get('initramfs', True):
            generators = vars['initramfs-generators']
            config_entry_commands_initramfs = self.config.merge('commands-image-initramfs-generators', arch, featureset, flavour)
            commands = [config_entry_commands_initramfs[i] for i in generators if config_entry_commands_initramfs.has_key(i)]
            makeflags['INITRD_CMD'] = ' '.join(commands)
            l_depends = PackageRelationGroup()
            for i in generators:
                i = config_entry_relations.get(i, i)
                l_depends.append(i)
                a = PackageRelationEntry(i)
                if a.operator is not None:
                    a.operator = -a.operator
                    image_relations['conflicts'].append(PackageRelationGroup([a]))
            image_relations['depends'].append(l_depends)

        packages_dummy = []
        packages_own = []

        if vars['type'] == 'plain-s390-tape':
            image = self.templates["control.image.type-standalone"]
            build_modules = False
        elif vars['type'] == 'plain-xen':
            image = self.templates["control.image.type-modulesextra"]
            build_modules = True
            config_entry_xen = self.config.merge('xen', arch, featureset, flavour)
            if config_entry_xen.get('dom0-support', True):
                p = self.process_packages(self.templates['control.xen-linux-system'], vars)
                l = PackageRelationGroup()
                for version in config_entry_xen['versions']:
                    l.append("xen-hypervisor-%s-%s" % (version, config_entry_xen['flavour']))
                makeflags['XEN_VERSIONS'] = ' '.join(['%s-%s' % (i, config_entry_xen['flavour']) for i in config_entry_xen['versions']])
                p[0]['Depends'].append(l)
                packages_dummy.extend(p)
        else:
            build_modules = True
            image = self.templates["control.image.type-%s" % vars['type']]
            #image = self.templates["control.image.type-modulesinline"]

        if not vars.has_key('desc'):
            vars['desc'] = None

        packages_own.append(self.process_real_image(image[0], image_relations, config_entry_relations, vars))
        packages_own.extend(self.process_packages(image[1:], vars))

        if build_modules:
            makeflags['MODULES'] = True
            package_headers = self.process_package(headers[0], vars)
            package_headers['Depends'].extend(relations_compiler)
            packages_own.append(package_headers)
            extra['headers_arch_depends'].append('%s (= ${Source-Version})' % packages_own[-1]['Package'])

        for package in packages_own + packages_dummy:
            name = package['Package']
            if packages.has_key(name):
                package = packages.get(name)
                package['Architecture'].append(arch)
            else:
                package['Architecture'] = [arch]
                packages.append(package)

        if vars['type'] == 'plain-xen':
            for i in ('postinst', 'postrm', 'prerm'):
                j = self.substitute(self.templates["image.xen.%s" % i], vars)
                file("debian/%s.%s" % (packages_own[0]['Package'], i), 'w').write(j)

        def get_config(*entry_name):
            entry_real = ('image',) + entry_name
            entry = self.config.get(entry_real, None)
            if entry is None:
                return None
            return entry.get('configs', None)

        def check_config_default(fail, f):
            for d in self.config_dirs[::-1]:
                f1 = d + '/' + f
                if os.path.exists(f1):
                    return [f1]
            if fail:
                raise RuntimeError("%s unavailable" % f)
            return []

        def check_config_files(files):
            ret = []
            for f in files:
                for d in self.config_dirs[::-1]:
                    f1 = d + '/' + f
                    if os.path.exists(f1):
                        ret.append(f1)
                        break
                else:
                    raise RuntimeError("%s unavailable" % f)
            return ret

        def check_config(default, fail, *entry_name):
            configs = get_config(*entry_name)
            if configs is None:
                return check_config_default(fail, default)
            return check_config_files(configs)

        kconfig = check_config('config', True)
        kconfig.extend(check_config("%s/config" % arch, True, arch))
        kconfig.extend(check_config("%s/config.%s" % (arch, flavour), False, arch, None, flavour))
        kconfig.extend(check_config("featureset-%s/config" % featureset, False, None, featureset))
        kconfig.extend(check_config("%s/%s/config" % (arch, featureset), False, arch, featureset))
        kconfig.extend(check_config("%s/%s/config.%s" % (arch, featureset, flavour), False, arch, featureset, flavour))
        makeflags['KCONFIG'] = ' '.join(kconfig)

        cmds_binary_arch = []
        cmds_binary_arch.append("$(MAKE) -f debian/rules.real binary-arch-flavour %s" % makeflags)
        if packages_dummy:
            cmds_binary_arch.append("$(MAKE) -f debian/rules.real install-dummy DH_OPTIONS='%s' %s" % (' '.join(["-p%s" % i['Package'] for i in packages_dummy]), makeflags))
        cmds_build = ["$(MAKE) -f debian/rules.real build %s" % makeflags]
        cmds_setup = ["$(MAKE) -f debian/rules.real setup-flavour %s" % makeflags]
        makefile.add('binary-arch_%s_%s_%s_real' % (arch, featureset, flavour), cmds = cmds_binary_arch)
        makefile.add('build_%s_%s_%s_real' % (arch, featureset, flavour), cmds = cmds_build)
        makefile.add('setup_%s_%s_%s_real' % (arch, featureset, flavour), cmds = cmds_setup)

    def do_extra(self, packages, makefile):
        apply = self.templates['patch.apply']
        unpatch = self.templates['patch.unpatch']

        vars = {
            'home': '/usr/src/kernel-patches/all/%s/debian' % self.version.linux_upstream,
            'revisions': ' '.join([i.debian for i in self.versions[::-1]]),
            'source': "%(linux_upstream)s-%(debian)s" % self.version.__dict__,
            'upstream': self.version.linux_upstream,
        }

        apply = self.substitute(apply, vars)
        unpatch = self.substitute(unpatch, vars)

        file('debian/bin/patch.apply', 'w').write(apply)
        file('debian/bin/patch.unpatch', 'w').write(unpatch)

    def process_changelog(self):
        act_upstream = self.changelog[0].version.linux_upstream
        versions = []
        for i in self.changelog:
            if i.version.linux_upstream != act_upstream:
                break
            versions.append(i.version)
        self.versions = versions
        self.version = self.changelog[0].version
        if self.version.linux_modifier is not None:
            self.abiname = ''
        else:
            self.abiname = '-%s' % self.config['abi',]['abiname']
        self.vars = self.process_version_linux(self.version, self.abiname)
        self.config['version',] = {'source': self.version.complete, 'abiname': self.abiname}

    def process_real_image(self, in_entry, relations, config, vars):
        entry = self.process_package(in_entry, vars)
        for field in 'Depends', 'Provides', 'Suggests', 'Recommends', 'Conflicts':
            value = entry.get(field, PackageRelation())
            t = vars.get(field.lower(), [])
            value.extend(t)
            t = relations.get(field.lower(), [])
            value.extend(t)
            if value:
                entry[field] = value
        return entry

    def process_real_tree(self, in_entry, vars):
        entry = self.process_package(in_entry, vars)
        for i in (('Depends', 'Provides')):
            value = PackageRelation()
            value.extend(entry.get(i, []))
            if i == 'Depends':
                v = self.changelog[0].version
                value.append("linux-patch-debian-%s (= %s)" % (v.linux_version, v.complete))
                value.append(' | '.join(["linux-source-%s (= %s)" % (v.linux_version, v.complete) for v in self.versions]))
            elif i == 'Provides':
                value.extend(["linux-tree-%s" % v.complete.replace('~', '-') for v in self.versions])
            entry[i] = value
        return entry

    def write(self, packages, makefile):
        self.write_config()
        super(Gencontrol, self).write(packages, makefile)

    def write_config(self):
        f = file("debian/config.defines.dump", 'w')
        self.config.dump(f)
        f.close()

if __name__ == '__main__':
    Gencontrol()()
