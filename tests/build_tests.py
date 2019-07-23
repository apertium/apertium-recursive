#!/usr/bin/env python3
f = open('run_tests.py', 'w')
f.write('''#!/usr/bin/env python3
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
    def setUp(self):
        subprocess.check_output(['../src/rtx-comp', self.rules_file, self.bin_file],
                                stderr=subprocess.STDOUT, universal_newlines=True)
    def test_output(self):
        actual = subprocess.check_output(['../src/rtx-proc', '-a', self.bin_file],
                                         input=self.input, encoding='utf-8', universal_newlines=True)
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

from os import listdir
ls = listdir('.')
for fname in ls:
    if fname.endswith('.rtx'):
        base = fname.split('.')[0]
        if (base + '.input') in ls:
            fi = open(base + '.input')
            i = fi.read()
            fi.close()
            fo = open(base + '.output')
            o = fo.read()
            fo.close()
            f.write(run.format(base, i, o))
        else:
            f.write(err.format(base))
f.write('''
if __name__ == '__main__':
    unittest.main(buffer=True, verbosity=2)
''')
