#!/usr/bin/env python3
f = open('run_tests.py', 'w')
f.write('''#!/usr/bin/env python3

#####################################################
### run_tests.py IS A GENERATED FILE, DO NOT EDIT ###
#####################################################

import subprocess, unittest

class CompilerTest:
    rules_file = ''
    bin_file = ''
    def test_compiles(self):
        with self.assertRaises(subprocess.CalledProcessError):
            subprocess.check_output(['../src/rtx-comp', self.rules_file, self.bin_file],
                                    stderr=subprocess.STDOUT, universal_newlines=True)

class InterpreterTest:
    rules_file = ''
    bin_file = ''
    input = ''
    output = ''
    lex_file = ''
    def setUp(self):
        args = ['../src/rtx-comp']
        if len(self.lex_file) > 0:
            args += ['-l', self.lex_file]
        args += [self.rules_file, self.bin_file]
        subprocess.check_output(args, stderr=subprocess.STDOUT, universal_newlines=True)
    def test_output(self):
        args = ['../src/rtx-proc', '-a', self.bin_file]
        actual = subprocess.check_output(args, input=self.input, universal_newlines=True)
        self.maxDiff = None
        self.assertEqual(self.output, actual)


''')

err = '''
class {0}(CompilerTest, unittest.TestCase):
    rules_file = '{0}.rtx'
    bin_file = '{0}.bin'
'''

run = '''
class {0}(InterpreterTest, unittest.TestCase):
    rules_file = '{0}.rtx'
    bin_file = '{0}.bin'
    input = """{1}"""
    output = """{2}"""
'''

run_xml = '''
class {0}XML(InterpreterTest, unittest.TestCase):
    rules_file = '{0}.trx'
    bin_file = '{0}.bin'
    input = """{1}"""
    output = """{2}"""
'''

cook = '''
class Cookbook{0}(InterpreterTest, unittest.TestCase):
    rules_file = 'cookbook/{0}.rtx'
    bin_file = 'cookbook/{0}.bin'
    input = """{1}"""
    output = """{2}"""
'''

from os import listdir
from os.path import basename
ls = listdir('.')
for fname in ls:
    if fname.endswith('.rtx') or fname.endswith('.trx'):
        base, ext = fname.split('.')
        if (base + '.input') in ls:
            fi = open(base + '.input')
            i = fi.read()
            fi.close()
            fo = open(base + '.output')
            o = fo.read()
            fo.close()
            if ext == 'trx':
                f.write(run_xml.format(base, i, o))
            else:
                f.write(run.format(base, i, o))
            if (base + '.lex') in ls:
                f.write("    lex_file = '%s.lex'\n" % base)
        else:
            f.write(err.format(base))
for fname in listdir('./cookbook'):
    if fname.endswith('.rtx'):
        base = fname.split('.')[0]
        rf = open('cookbook/' + fname)
        i = ''
        o = ''
        for l in rf.readlines():
            if l.startswith('! < '): i += l[4:]
            elif l.startswith('! > '): o += l[4:]
        f.write(cook.format(base, i, o))
f.write('''
if __name__ == '__main__':
    unittest.main(buffer=True, verbosity=2)
''')
