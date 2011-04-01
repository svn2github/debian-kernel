use strict;
use warnings;
use Test;

use DebianLinux qw(version_cmp);

BEGIN {
    plan test => 27;
}

# Simple numeric comparison
ok(version_cmp('2', '2'), 0);
ok(version_cmp('2', '3'), -1);
ok(version_cmp('3', '2'), 1);
# Multiple components
ok(version_cmp('2.6.32', '2.6.32'), 0);
ok(version_cmp('2.6.32', '2.6.33'), -1);
ok(version_cmp('2.6.33', '2.6.32'), 1);
# Extra components (non-numeric, non-pre-release) > null
ok(version_cmp('2.6.32-local', '2.6.32-local'), 0);
ok(version_cmp('2.6.32', '2.6.32-local'), -1);
ok(version_cmp('2.6.32-local', '2.6.32'), 1);
# Extra numeric components > null
ok(version_cmp('2.6.32', '2.6.32.1'), -1);
ok(version_cmp('2.6.32.1', '2.6.32'), 1);
ok(version_cmp('2.6.32', '2.6.32-1'), -1);
ok(version_cmp('2.6.32-1', '2.6.32'), 1);
# Extra pre-release components < null
ok(version_cmp('2.6.33-rc1', '2.6.33-rc1'), 0);
ok(version_cmp('2.6.33-rc1', '2.6.33'), -1);
ok(version_cmp('2.6.33', '2.6.33-rc1'), 1);
ok(version_cmp('2.6.33-trunk', '2.6.33-trunk'), 0);
ok(version_cmp('2.6.33-rc1', '2.6.33-trunk'), -1);
ok(version_cmp('2.6.33-trunk', '2.6.33'), -1);
# Pre-release < numeric
ok(version_cmp('2.6.32-1', '2.6.32-trunk'), 1);
ok(version_cmp('2.6.32-trunk', '2.6.32-1'), -1);
# Pre-release < non-numeric non-pre-release
ok(version_cmp('2.6.32-local', '2.6.32-trunk'), 1);
ok(version_cmp('2.6.32-trunk', '2.6.32-local'), -1);
# Numeric < non-numeric non-pre-release
ok(version_cmp('2.6.32-1', '2.6.32-local'), -1);
ok(version_cmp('2.6.32-local', '2.6.32-1'), 1);
# Hyphen < dot
ok(version_cmp('2.6.32-2', '2.6.32.1'), -1);
ok(version_cmp('2.6.32.1', '2.6.32-2'), 1);
