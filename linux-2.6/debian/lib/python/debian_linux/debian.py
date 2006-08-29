import itertools, os.path, re, utils

def read_changelog(dir = ''):
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
    f = file(os.path.join(dir, "debian/changelog"))
    entries = []
    act_upstream = None
    while True:
        line = f.readline()
        if not line:
            break
        line = line.strip('\n')
        match = r.match(line)
        if not match:
            continue
        if match.group('header'):
            e = {}
            e['Distribution'] = match.group('header_distribution')
            e['Source'] = match.group('header_source')
            version = parse_version(match.group('header_version'))
            e['Version'] = version
            if act_upstream is None:
                act_upstream = version['upstream']
            elif version['upstream'] != act_upstream:
                break
            entries.append(e)
    return entries

def parse_version(version):
    ret = {
        'complete': version,
        'upstream': version,
        'debian': None,
        'linux': None,
    }
    try:
        i = len(version) - version[::-1].index('-')
    except ValueError:
        return ret
    ret['upstream'] = version[:i-1]
    ret['debian'] = version[i:]
    try:
        ret['linux'] = parse_version_linux(version)
    except ValueError:
        pass
    return ret

def parse_version_linux(version):
    version_re = ur"""
^
(?P<source>
    (?P<parent>
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
    if match is None:
        raise ValueError
    ret = match.groupdict()
    if ret['parent'] is not None:
        ret['source_upstream'] = ret['parent'] + ret['upstream']
    else:
        ret['source_upstream'] = ret['upstream']
    return ret

class package_description(object):
    __slots__ = "short", "long"

    def __init__(self, value = None):
        self.long = []
        if value is not None:
            self.short, long = value.split("\n", 1)
            self.append(long)
        else:
            self.short = None

    def __str__(self):
        ret = self.short + '\n'
        w = utils.wrap(width = 74, fix_sentence_endings = True)
        pars = []
        for i in self.long:
            pars.append('\n '.join(w.wrap(i)))
        return self.short + '\n ' + '\n .\n '.join(pars)

    def append(self, str):
        str = str.strip()
        if str:
            self.long.extend(str.split("\n.\n"))

class package_relation(object):
    __slots__ = "name", "version", "arches"

    _re = re.compile(r'^(\S+)(?: \(([^)]+)\))?(?: \[([^]]+)\])?$')

    def __init__(self, value = None):
        if value is not None:
            self.parse(value)
        else:
            self.name = None
            self.version = None
            self.arches = []

    def __str__(self):
        ret = [self.name]
        if self.version is not None:
            ret.extend([' (', self.version, ')'])
        if self.arches:
            ret.extend([' [', ' '.join(self.arches), ']'])
        return ''.join(ret)

    def config(self, entry):
        if self.version is not None or self.arches:
            return
        value = entry.get(self.name, None)
        if value is None:
            return
        self.parse(value)

    def parse(self, value):
        match = self._re.match(value)
        if match is None:
            raise RuntimeError, "Can't parse dependency %s" % value
        match = match.groups()
        self.name = match[0]
        self.version = match[1]
        if match[2] is not None:
            self.arches = re.split('\s+', match[2])
        else:
            self.arches = []

class package_relation_list(list):
    def __init__(self, value = None):
        if value is not None:
            self.extend(value)

    def __str__(self):
        return ', '.join([str(i) for i in self])

    def _match(self, value):
        for i in self:
            if i._match(value):
                return i
        return None

    def append(self, value):
        if isinstance(value, basestring):
            value = package_relation_group(value)
        elif not isinstance(value, package_relation_group):
            raise ValueError, "got %s" % type(value)
        j = self._match(value)
        if j:
            j._update_arches(value)
        else:
            super(package_relation_list, self).append(value)

    def config(self, entry):
        for i in self:
            i.config(entry)

    def extend(self, value):
        if isinstance(value, basestring):
            value = [j.strip() for j in re.split(',', value.strip())]
        elif not isinstance(value, (list, tuple)):
            raise ValueError, "got %s" % type(value)
        for i in value:
            self.append(i)

class package_relation_group(list):
    def __init__(self, value = None):
        if value is not None:
            self.extend(value)

    def __str__(self):
        return ' | '.join([str(i) for i in self])

    def _match(self, value):
        for i, j in itertools.izip(self, value):
            if i.name != j.name or i.version != j.version:
                return None
        return self

    def _update_arches(self, value):
        for i, j in itertools.izip(self, value):
            if i.arches:
                for arch in j.arches:
                    if arch not in i.arches:
                        i.arches.append(arch)

    def append(self, value):
        if isinstance(value, basestring):
            value = package_relation(value)
        elif not isinstance(value, package_relation):
            raise ValueError
        super(package_relation_group, self).append(value)

    def config(self, entry):
        for i in self:
            i.config(entry)

    def extend(self, value):
        if isinstance(value, basestring):
            value = [j.strip() for j in re.split('\|', value.strip())]
        elif not isinstance(value, (list, tuple)):
            raise ValueError
        for i in value:
            self.append(i)

class package(dict):
    _fields = utils.sorted_dict((
        ('Package', str),
        ('Source', str),
        ('Architecture', utils.field_list),
        ('Section', str),
        ('Priority', str),
        ('Maintainer', str),
        ('Uploaders', str),
        ('Standards-Version', str),
        ('Build-Depends', package_relation_list),
        ('Build-Depends-Indep', package_relation_list),
        ('Provides', package_relation_list),
        ('Depends', package_relation_list),
        ('Recommends', package_relation_list),
        ('Suggests', package_relation_list),
        ('Replaces', package_relation_list),
        ('Conflicts', package_relation_list),
        ('Description', package_description),
    ))

    def __setitem__(self, key, value):
        try:
            cls = self._fields[key]
            if not isinstance(value, cls):
                value = cls(value)
        except KeyError: pass
        super(package, self).__setitem__(key, value)

    def iterkeys(self):
        keys = set(self.keys())
        for i in self._fields.iterkeys():
            if self.has_key(i):
                keys.remove(i)
                yield i
        for i in keys:
            yield i

    def iteritems(self):
        keys = set(self.keys())
        for i in self._fields.iterkeys():
            if self.has_key(i):
                keys.remove(i)
                yield (i, self[i])
        for i in keys:
            yield (i, self[i])

    def itervalues(self):
        keys = set(self.keys())
        for i in self._fields.iterkeys():
            if self.has_key(i):
                keys.remove(i)
                yield self[i]
        for i in keys:
            yield self[i]

