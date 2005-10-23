#!/usr/bin/env python
import os, os.path, re, sys, textwrap, ConfigParser
sys.path.append("debian/lib/python")
from debian_linux import *

class packages_list(sorted_dict):
    def append(self, package):
        self[package['Package']] = package

    def extend(self, packages):
        for package in packages:
            self[package['Package']] = package

def read_changelog():
    r = re.compile(r"""
^
(
(?P<header>
    (?P<header_source>
        \w[-+0-9a-z.]+
    )
    \ 
    \(
    (?P<header_version>
        [^\(\)\ \t]+
    )
    \)
    \s+
    (?P<header_distribution>
        [-0-9a-zA-Z]+
    )
    \;
)
)
""", re.VERBOSE)
    f = file("debian/changelog")
    entries = []
    while True:
        line = f.readline()
        if not line:
            break
        line = line.strip('\n')
        match = r.match(line)
        if not match:
            continue
        if match.group('header'):
            e = entry()
            e['Distribution'] = match.group('header_distribution')
            e['Source'] = match.group('header_source')
            e['Version'] = parse_version(match.group('header_version'))
            entries.append(e)
    return entries

def read_rfc822(f):
    entries = []

    while True:
        e = entry()
        while True:
            line = f.readline()
            if not line:
                break
            line = line.strip('\n')
            if not line:
                break
            if line[0] in ' \t':
                if not last:
                    raise ValueError('Continuation line seen before first header')
                e[last] += '\n' + line.lstrip()
                continue
            i = line.find(':')
            if i < 0:
                raise ValueError("Not a header, not a continuation: ``%s''" % line)
            last = line[:i]
            e[last] = line[i+1:].lstrip()
        if not e:
            break

        entries.append(e)

    return entries

def read_template(name):
    return read_rfc822(file("debian/templates/control.%s.in" % name))

def parse_version(version):
    version_re = ur"""
^
(?P<source>
    (?:
        \d+\.\d+\.\d+\+
    )?
    (?P<upstream>
        (?P<version>
            (?P<major>\d+\.\d+)
            \.
            \d+
        )
        (?:
            -
            (?P<modifier>
                .+?
            )
        )?
    )
    -
    (?P<debian>[^-]+)
)
$
"""
    match = re.match(version_re, version, re.X)
    return match.groupdict()

def process_changelog(in_vars, changelog):
    ret = [None, None, None, None]
    ret[0] = version = changelog[0]['Version']
    vars = in_vars.copy()
    if version['modifier'] is not None:
        ret[1] = vars['abiname'] = version['modifier']
        ret[2] = ""
    else:
        ret[1] = vars['abiname'] = c['base']['abiname']
        ret[2] = "-%s" % vars['abiname']
    vars['version'] = version['version']
    vars['major'] = version['major']
    ret[3] = vars
    return ret

def process_depends(key, e, in_e, vars):
    in_dep = in_e[key].split(',')
    dep = []
    for d in in_dep:
        d = d.strip()
        d = substitute(d, vars)
        if d:
            dep.append(d)
    if dep:
        t = ', '.join(dep)
        e[key] = t

def process_description(e, in_e, vars):
    desc = in_e['Description']
    desc_short, desc_long = desc.split ("\n", 1)
    desc_pars = [substitute(i, vars) for i in desc_long.split ("\n.\n")]
    desc_pars_wrapped = []
    w = wrap(width = 74, fix_sentence_endings = True)
    for i in desc_pars:
        desc_pars_wrapped.append(w.fill(i))
    e['Description'] = "%s\n%s" % (substitute(desc_short, vars), '\n.\n'.join(desc_pars_wrapped))

def process_package(in_entry, vars):
    e = entry()
    for i in in_entry.iterkeys():
        if i in (('Depends', 'Provides', 'Suggests')):
            process_depends(i, e, in_entry, vars)
        elif i == 'Description':
            process_description(e, in_entry, vars)
        elif i[:2] == 'X-':
            pass
        else:
            e[i] = substitute(in_entry[i], vars)
    return e

def process_packages(in_entries, vars):
    entries = []
    for i in in_entries:
        entries.append(process_package(i, vars))
    return entries

def substitute(s, vars):
    def subst(match):
        return vars[match.group(1)]
    return re.sub(r'@([a-z_]+)@', subst, s)

def write_control(list):
    write_rfc822(file("debian/control", 'w'), list)

def write_makefile(list):
    f = file("debian/rules.gen", 'w')
    for i in list:
        f.write("%s\n" % i[0])
        if i[1] is not None:
            list = i[1]
            if isinstance(list, basestring):
                list = list.split('\n')
            for j in list:
                f.write("\t%s\n" % j)

def write_rfc822(f, list):
    for entry in list:
        for key, value in entry.iteritems():
            f.write("%s:" % key)
            if isinstance(value, tuple):
                value = value[0].join(value[1])
            for k in value.split('\n'):
              f.write(" %s\n" % k)
        f.write('\n')

def process_real_arch(packages, makefile, config, arch, vars, makeflags):
    config_entry = config[arch]
    vars.update(config_entry)

    if not config_entry.get('available', True):
        for i in ('binary-arch', 'build'):
            makefile.append(("%s-%s:" % (i, arch), ["@echo Architecture %s is not available!" % arch, "@exit 1"]))
        return

    makeflags['ARCH'] = arch

    for subarch in config_entry['subarches']:
        process_real_subarch(packages, makefile, config, arch, subarch, vars.copy(), makeflags.copy())

def process_real_flavour(packages, makefile, config, arch, subarch, flavour, vars, makeflags):
    config_entry = config['-'.join((arch, subarch, flavour))]
    vars.update(config_entry)

    vars['flavour'] = flavour
    if not vars.has_key('class'):
        vars['class'] = '%s-class' % flavour
    if not vars.has_key('longclass'):
        vars['longclass'] = vars['class']

    modules = read_template("modules")

    package = process_package(modules[0], vars)

    name = package['Package']
    if packages.has_key(name):
        package = packages.get(name)
        package['Architecture'][1].append(arch)
    else:
        package['Architecture'] = (' ', [arch])
        packages.append(package)

    for i in ('binary-arch', 'build'):
        makefile.append(("%s-%s-%s:: %s-%s-%s-%s" % (i, arch, subarch, i, arch, subarch, flavour), None))
        makefile.append(("%s-%s-%s-%s:: %s-%s-%s-%s-real" % (i, arch, subarch, flavour, i, arch, subarch, flavour), None))

    makeflags['FLAVOUR'] = flavour
    if config_entry.has_key('kernel-arch'):
        makeflags['KERNEL_ARCH'] = config_entry['kernel-arch']
    makeflags_string = ' '.join(["%s='%s'" % i for i in makeflags.iteritems()])

    cmds_binary_arch = []
    cmds_binary_arch.append(("$(MAKE) -f debian/rules.real binary-arch-flavour %s" % makeflags_string,))
    cmds_build = []
    cmds_build.append(("$(MAKE) -f debian/rules.real build %s" % makeflags_string,))
    makefile.append(("binary-arch-%s-%s-%s-real:" % (arch, subarch, flavour), cmds_binary_arch))
    makefile.append(("build-%s-%s-%s-real:" % (arch, subarch, flavour), cmds_build))

def process_real_main(packages, makefile, config, version, abiname, kpkg_abiname, changelog, vars):
    source = read_template("source")
    packages['source'] = process_package(source[0], vars)

    main = read_template("main")
    packages.extend(process_packages(main, vars))

    makeflags = {
        'VERSION': version['version'],
        'SOURCE_VERSION': version['source'],
        'UPSTREAM_VERSION': version['upstream'],
        'ABINAME': abiname,
    }
    makeflags_string = ' '.join(["%s='%s'" % i for i in makeflags.iteritems()])

    cmds_binary_indep = []
    cmds_binary_indep.append(("$(MAKE) -f debian/rules.real binary-indep %s" % makeflags_string,))
    makefile.append(("binary-indep:", cmds_binary_indep))

    for arch in iter(config['base']['arches']):
        process_real_arch(packages, makefile, config, arch, vars.copy(), makeflags.copy())

def process_real_subarch(packages, makefile, config, arch, subarch, vars, makeflags):
    if subarch == 'none':
        vars['subarch'] = ''
        config_entry = config[arch]
    else:
        vars['subarch'] = '%s-' % subarch
        config_entry = config['%s-%s' % (arch, subarch)]
    vars.update(config_entry)

    for i in ('binary-arch', 'build'):
        makefile.append(("%s-%s:: %s-%s-%s" % (i, arch, i, arch, subarch), None))
        makefile.append(("%s-%s-%s::" % (i, arch, subarch), None))

    makeflags['SUBARCH'] = subarch
    if config_entry.has_key('kernel-arch'):
        makeflags['KERNEL_ARCH'] = config_entry['kernel-arch']

    for flavour in config_entry['flavours']:
        process_real_flavour(packages, makefile, config, arch, subarch, flavour, vars.copy(), makeflags.copy())

def main():
    changelog = read_changelog()

    version, abiname, kpkg_abiname, vars = process_changelog({}, changelog)

    c = config("/usr/src/linux-headers-%s" % version['version'])

    packages = packages_list()
    makefile = []

    process_real_main(packages, makefile, c, version, abiname, kpkg_abiname, changelog, vars)

    write_control(packages.itervalues())
    write_makefile(makefile)


if __name__ == '__main__':
    main()
