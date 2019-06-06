#!usr/bin/env python3
import sys
f = open(sys.argv[1])
s = f.read()
f.close()
out = []

out.append('Longest pattern: %s' % ord(s[0]))
out.append('')
rulecount = ord(s[1])
out.append('Number of rules: %s' % rulecount)

intarg = {'j':'jump', '?':'jump-if-not', '&':'and', '|':'or', '<':'out', '.':'clip', '+':'concat', '{':'chunk', '_':'blank'}
caseless = {'=':'equal', '(':'begins-with', ')':'ends-with', '[':'begins-with-list', ']':'ends-with-list', 'c':'contains', 'n':'in'}
other = {'!':'not', '>':'begin-let', '$':'var', 'G':'case-of', 'A':'copy-case', 'p':'pseudolemma', ' ':'space'}

s = s[2:]
for i in range(rulecount):
    ln = ord(s[0])
    out.append('')
    out.append('Rule %s - %s instructions' % (i+1, ln))
    rl = s[1:1+ln]
    s = s[1+ln:]
    for j in range(len(rl)):
        c = rl[j]
        if c in intarg:
            j += 1
            out.append('[%s]: %s (%s) %s' % (j, c, intarg[c], ord(rl[j])))
        elif c in caseless:
            name = caseless[c]
            if j+1 < len(rl) and rl[j+1] == '#':
                if c in '*4':
                    n += ' case'
                else:
                    name += ' caseless'
                j += 1
            out.append('[%s]: %s (%s)' % (j, c, name))
        elif c in other:
            out.append('[%s]: %s (%s)' % (j, c, other[c]))
        elif c == 's':
            j += 1
            n = ord(rl[j])
            out.append('[%s]: s (string) "%s"' % (j, rl[j+1:j+n+1]))
            j += n

f = open(sys.argv[2])
f.write('\n'.join(out))
f.close()
