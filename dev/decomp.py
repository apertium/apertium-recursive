#!/usr/bin/env python3
import sys
f = open(sys.argv[1])
s = f.read()
f.close()
out = []

out.append('Longest pattern: %s' % ord(s[0]))
out.append('')
rulecount = ord(s[1])
out.append('Number of rules: %s' % rulecount)

intarg = {'j':'jump', '?':'jump-if-not', '&':'and', '|':'or', '<':'out', '.':'clip', '+':'concat', '{':'chunk', '_':'blank', 'S':'source clip', 'R':'ref clip', 'T':'target clip', 't':'set clip'}
caseless = {'=':'equal', '(':'begins-with', ')':'ends-with', '[':'begins-with-list', ']':'ends-with-list', 'c':'contains', 'n':'in'}
other = {'!':'not', '>':'begin-let', '$':'var', 'G':'case-of', 'A':'copy-case', 'p':'pseudolemma', ' ':'space'}

s = s[2:]
for i in range(rulecount):
    ln = ord(s[0])
    out.append('')
    out.append('Rule %s - %s instructions' % (i+1, ln))
    rl = s[1:1+ln]
    s = s[1+ln:]
    skip = 0
    for j in range(len(rl)):
        if skip > 0:
            skip -= 1
            continue
        c = rl[j]
        ln = '[%s]: %s ' % (j, c)
        if c in intarg:
            ln += '(%s) %s' % (intarg[c], ord(rl[j+1]))
            skip = 1
        elif c in caseless:
            name = caseless[c]
            if j+1 < len(rl) and rl[j+1] == '#':
                skip = 1
                if c in '*4':
                    n += ' case'
                else:
                    name += ' caseless'
                j += 1
            ln += '(%s)' % name
        elif c in other:
            ln += '(%s)' % other[c]
        elif c == 's':
            n = ord(rl[j+1])
            ln += '(string) "%s"' % rl[j+2:j+n+2]
            skip = n+1
        out.append(ln)

f = open(sys.argv[2], 'w')
f.write('\n'.join(out))
f.close()
