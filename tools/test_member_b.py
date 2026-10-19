#!/usr/bin/env python3
"""Build and test A/B/C against a new disposable database, never the project database."""
import argparse
import datetime
import os
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--name', default='member_b_test_' + datetime.datetime.now().strftime('%Y%m%d%H%M%S'))
    args = parser.parse_args()
    if not re.fullmatch(r'member_b_test_[a-zA-Z0-9_]+', args.name):
        parser.error('database name must start with member_b_test_ and use letters/digits/underscore')
    name = args.name
    env = os.environ.copy()
    env['MYSQL_PWD'] = env.get('B_DB_PASSWORD', '')
    command = ['mysql', '--default-character-set=utf8mb4', '--host=' + env.get('B_DB_HOST', 'localhost'),
               '--user=' + env.get('B_DB_USER', 'root')]
    if env.get('MYSQL_TCP_PORT'):
        command.append('--port=' + env['MYSQL_TCP_PORT'])

    def sql(text, selected=None):
        subprocess.run(command + ([selected] if selected else []), input=text.encode('utf-8'), env=env, check=True)

    # Refuse reuse: no IF NOT EXISTS and no DROP DATABASE.
    sql('CREATE DATABASE `' + name + '` CHARACTER SET utf8mb4;')
    print('Disposable database created:', name, flush=True)
    sql((ROOT / 'sql/init_db.sql').read_text(encoding='utf-8').replace('medical_system', name))
    for filename in ['member_a_upgrade.sql', 'member_b_upgrade.sql',
                     'member_a_upgrade.sql', 'member_b_upgrade.sql']:
        sql((ROOT / 'sql' / filename).read_text(encoding='utf-8'), name)
    subprocess.run(['make', 'build/test_member_b'], cwd=ROOT, check=True)
    env.update(B_DB_NAME=name, B_TEST_ALLOW_WRITE='YES')
    result = subprocess.run([str(ROOT / 'build/test_member_b')], cwd=ROOT, env=env)
    print('Test database retained for inspection:', name)
    print('Delete only when no longer needed: DROP DATABASE `' + name + '`;')
    return result.returncode

if __name__ == '__main__':
    try:
        sys.exit(main())
    except subprocess.CalledProcessError as exc:
        print('Command failed; disposable database retained for diagnosis.', file=sys.stderr)
        sys.exit(exc.returncode)
