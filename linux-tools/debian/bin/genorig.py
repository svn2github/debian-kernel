#!/usr/bin/env python

import sys
sys.path.append("debian/lib/python")

import os
import os.path
import re
import shutil
import subprocess

from debian_linux.debian import Changelog, VersionLinux


class FileGlob(object):
    def __init__(self, root):
        self.root = root or os.curdir

    def __call__(self, pathname):
        dirname, basename = os.path.split(pathname)
        if dirname:
            dirs = self(dirname)
        else:
            dirs = ('', )
        for d in dirs:
            if basename:
                for name in self.glob(d, basename):
                    yield os.path.join(d, name)
            elif self.isdir(d):
                yield os.path.join(d, '')

    def glob(self, dirname, pattern):
        import fnmatch
        try:
            names = self.listdir(dirname)
        except os.error:
            return ()
        if pattern[0] != '.':
            names = filter(lambda x: x[0] != '.', names)
        return fnmatch.filter(names, pattern)

    def isdir(self, dirname):
        return os.path.isdir(os.path.join(self.root, dirname))

    def listdir(self, dirname):
        return os.listdir(os.path.join(self.root, dirname))


class Main(object):
    def __init__(self, input_files, override_version, override_tag):
        self.log = sys.stdout.write

        self.input_files = input_files

        changelog = Changelog(version = VersionLinux)[0]
        source = changelog.source
        version = changelog.version

        if override_version:
            version = VersionLinux('%s-undef' % override_version)

        self.version_dfsg = version.linux_dfsg
        if self.version_dfsg is None:
            self.version_dfsg = '0'

        self.log('Using source name %s, version %s, dfsg %s\n' % (source, version.upstream, self.version_dfsg))

        self.orig = '%s-%s' % (source, version.upstream)
        self.orig_tar = '%s_%s.orig.tar.gz' % (source, version.upstream)
        self.tag = override_tag or ('v' + version.upstream.replace('~', '-'))

    def __call__(self):
        import tempfile
        self.dir = tempfile.mkdtemp(prefix = 'genorig', dir = 'debian')
        try:
            if os.path.isdir(self.input_files[0]):
                self.upstream_export(self.input_files[0])
            else:
                self.upstream_extract(self.input_files[0])
            if len(self.input_files) > 1:
                self.upstream_patch(self.input_files[1])
            self.generate()
            self.tar()
        finally:
            shutil.rmtree(self.dir)

    def upstream_export(self, input_repo):
        self.log("Exporting %s from %s\n" % (self.tag, input_repo))

        archive_proc = subprocess.Popen(['git', 'archive', '--format=tar',
                                         '--prefix=temp/', self.tag],
                                        cwd=input_repo,
                                        stdout=subprocess.PIPE)
        extract_proc = subprocess.Popen(['tar', '-xf', '-'], cwd=self.dir,
                                        stdin=archive_proc.stdout)

        ret1 = archive_proc.wait()
        ret2 = extract_proc.wait()
        if ret1 or ret2:
            raise RuntimeError("Can't create archive")

    def upstream_extract(self, input_tar):
        self.log("Extracting tarball %s\n" % input_tar)
        match = re.match(r'(^|.*/)(?P<dir>linux-\d+\.\d+\.\d+(-\S+)?)\.tar(\.(?P<extension>(bz2|gz)))?$', input_tar)
        if not match:
            raise RuntimeError("Can't identify name of tarball")

        cmdline = ['tar', '-xf', input_tar, '-C', self.dir]
        if match.group('extension') == 'bz2':
            cmdline.append('-j')
        elif match.group('extension') == 'gz':
            cmdline.append('-z')

        if subprocess.Popen(cmdline).wait():
            raise RuntimeError("Can't extract tarball")

        os.rename(os.path.join(self.dir, match.group('dir')), os.path.join(self.dir, 'temp'))

    def upstream_patch(self, input_patch):
        self.log("Patching source with %s\n" % input_patch)
        match = re.match(r'(^|.*/)patch-\d+\.\d+\.\d+(-\S+?)?(\.(?P<extension>(bz2|gz)))?$', input_patch)
        if not match:
            raise RuntimeError("Can't identify name of patch")
        cmdline = []
        if match.group('extension') == 'bz2':
            cmdline.append('bzcat')
        elif match.group('extension') == 'gz':
            cmdline.append('zcat')
        else:
            cmdline.append('cat')
        cmdline.append(input_patch)
        cmdline.append('| (cd %s; patch -p1 -f -s -t --no-backup-if-mismatch)' % os.path.join(self.dir, 'temp'))
        if os.spawnv(os.P_WAIT, '/bin/sh', ['sh', '-c', ' '.join(cmdline)]):
            raise RuntimeError("Can't patch source")

    def generate(self):
        self.log("Generate orig\n")
        orig = os.path.join(self.dir, self.orig)
        temp = os.path.join(self.dir, 'temp')

        to_copy = (
                'COPYING',
                'Kbuild',
                'Makefile',
                'arch/*/include/',
                'arch/*/Makefile',
                'arch/x86/lib/memcpy_64.S',
                'include/',
                'lib/rbtree.c',
                'scripts/',
                'tools/',
        )

        glob = FileGlob(temp)
        for i in to_copy:
            for j in glob(i):
                temp_j = os.path.join(temp, j)
                orig_j = os.path.join(orig, j)
                if j.endswith('/'):
                    shutil.copytree(temp_j, orig_j)
                else:
                    d = os.path.dirname(orig_j)
                    if not os.path.exists(d):
                        os.makedirs(d)
                    shutil.copyfile(temp_j, orig_j)

    def tar(self):
        out = os.path.join("../orig", self.orig_tar)
        try:
            os.mkdir("../orig")
        except OSError: pass
        try:
            os.stat(out)
            raise RuntimeError("Destination already exists")
        except OSError: pass
        self.log("Generate tarball %s\n" % out)
        cmdline = ['tar -czf', out, '-C', self.dir, self.orig]
        try:
            if os.spawnv(os.P_WAIT, '/bin/sh', ['sh', '-c', ' '.join(cmdline)]):
                raise RuntimeError("Can't patch source")
            os.chmod(out, 0644)
        except:
            try:
                os.unlink(out)
            except OSError: pass
            raise
        try:
            os.symlink(os.path.join('orig', self.orig_tar), os.path.join('..', self.orig_tar))
        except OSError: pass

if __name__ == '__main__':
    from optparse import OptionParser
    parser = OptionParser(usage = "%prog [OPTION]... {TAR [PATCH] | REPO}")
    parser.add_option("-V", "--override-version", dest = "override_version", help = "Override version", metavar = "VERSION")
    parser.add_option("-t", "--override-tag", dest = "override_tag", help = "Override tag", metavar = "TAG")
    options, args = parser.parse_args()

    assert 1 <= len(args) <= 2
    Main(args, options.override_version, options.override_tag)()
