#!/usr/bin/env python3
import random, sys, os, subprocess, collections, tempfile, xml.etree.ElementTree as ET

class Rule:
    allRules = collections.defaultdict(list)
    dix = None
    mode = None # 'l' or 'r'
    trans = None

    def __init__(self, result, parts):
        self.result = result
        self.parts = parts
        Rule.allRules[' '.join(result)].append(self)

    @staticmethod
    def generate(node):
        # Keep track of selected rules to avoid repetition
        selected_rules = set()

        if node in Rule.allRules:
            while True:
                # Randomly select a rule without repetition
                rl = random.choice(Rule.allRules[node])
                if rl not in selected_rules:
                    selected_rules.add(rl)
                    break

            for c in rl.parts:
                yield from Rule.generate(c)
        else:
            ls = Rule.dix.findall(".//%s/s[@n='%s'].." % (Rule.mode, node))
            if len(ls) == 0:
                print('Unable to generate node %s' % node)
                sys.exit()
            n = random.choice(ls)
            s = ET.tostring(n, encoding='unicode')[3:-4].replace('<b />', '# ').replace('<s n="', '<').replace('" />', '>')
            proc = subprocess.run(['random-path', Rule.trans, s], stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True)
            yield '^' + proc.stdout.decode('utf-8').strip() + '$'

def name(s):
    return os.path.dirname(s) or os.path.basename(s)

if __name__ == '__main__':
    if len(sys.argv) != 4:
        print('Usage: %s start_node pair_dir source_language_dir' % sys.argv[0])
        sys.exit()

    node = sys.argv[1]
    pairdir = sys.argv[2]

    if not os.path.isdir(pairdir):
        print('Unable to access %s' % pairdir)
        sys.exit()

    langdir = sys.argv[3]

    if not os.path.isdir(langdir):
        print('Unable to access %s' % langdir)
        sys.exit()

    pairname = '-'.join(name(pairdir).split('-')[-2:])
    langname = name(langdir).split('-')[-1]
    trans = os.path.join(langdir, '%s.autogen.bin' % langname)

    if not os.path.isfile(trans):
        print('Unable to access transducer %s' % trans)
        sys.exit()

    Rule.trans = trans
    lang2 = pairname.replace(langname, '').replace('-', '')
    dix = os.path.join(pairdir, 'apertium-%s.%s.dix' % (pairname, pairname))

    if not os.path.isfile(dix):
        print('Unable to access dictionary %s' % dix)
        sys.exit()

    Rule.dix = ET.parse(dix).getroot()
    rtx = os.path.join(pairdir, 'apertium-%s.%s-%s.rtx' % (pairname, langname, lang2))

    if not os.path.isfile(rtx):
        print('Unable to access rule file %s' % rtx)
        sys.exit()

    rtxbin = os.path.join(pairdir, 'temp.randsen.rtx.bin')
    proc = subprocess.run(['rtx-comp', '-s', rtx, rtxbin], stderr=subprocess.PIPE)

    if proc.returncode != 0:
        print(['rtx-proc', '-s', rtx, rtxbin])
        print(proc.stderr)
        print(proc.returncode)
        print('Compilation of %s failed.' % rtx)
        sys.exit()

    os.remove(rtxbin)

    for line in proc.stderr.decode('utf-8').splitlines():
        if '->' not in line:
            continue

        o, p = line.strip().split('"')[-1].split(' -> ')
        Rule(o.split(), p.split())

    Rule.mode = 'l' if (pairname == langname + '-' + lang2) else 'r'

    for w in Rule.generate(node):
        print(w, end=' ')

    print('')
