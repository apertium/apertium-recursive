#!/usr/bin/env python3
import xml.etree.ElementTree as xml
import sys

listNames = []
attrNames = []
varNames = []
catNames = []
macros = {}

maxlen = 0

READABLE = False

def sint(num):
    if isinstance(num, str):
        num = int(num)
    if READABLE:
        return str(num)
    else:
        return chr(num)
def slen(ls):
    return sint(len(ls))

def toreadstr(blob):
    global listNames, attrNames, catNames, varNames, macros, maxlen
    kids = [toreadstr(x) for x in blob]
    kids = [k for k in kids if k and not k.isspace()]
    lines = '\n'.join(kids)
    ret = ''
    if blob.tag == 'pattern':
        maxlen = max(maxlen, len(kids))
    logic = {'not':'! (not)', 'equal':'= (equal)', 'begins-with':'( (begins-with)', 'ends-with':') (ends-with)', 'begins-with-list':'[ (begins-with-list)', 'ends-with-list':'] (ends-with-list)', 'contains-substring':'c (contains substring)', 'in':'n (in)', 'test':'? (test/jump-if-not) ', 'get-case-from':'G (get-case)'}
    stack = {'and':'& (and) ', 'or':'| (or) ', 'concat':'+ (concat) ', 'out':'< (out) ', 'chunk':'{ (chunk) '}
    def litstr(s):
        nonlocal ret
        if ret and ret[-1] != '\n':
            ret += '\n'
        ret += 's (str) %s %s' % (len(s), s)
    if blob.tag == 'interchunk':
        ret = 'Longest pattern: ' + sint(maxlen) + '\n\n' + '\n\n'.join(kids)
    elif blob.tag == 'section-rules':
        ret = 'Number of rules: ' + slen(kids) + '\n\n' + '\n\n'.join(kids)
    elif blob.tag == 'def-macro':
        macros[blob.attrib['n']] = ''.join(kids)
    elif blob.tag == 'rule':
        ret = 'R (rule) ' + slen(lines) + '\n' + lines
    elif blob.tag in logic:
        cs = logic[blob.tag]
        if blob.tag not in ['not', 'test', 'get-case-from']:
            if 'caseless' in blob.attrib and blob.attrib['caseless'] == 'yes':
                cs = cs[0] + '#' + cs[1:]
        ret = lines + '\n' + cs
    elif blob.tag in ['let', 'modify-case']:
        ret = '> (begin-let)\n' + lines + '\n'
        if blob[0].tag == 'clip':
            ret += '*'
        else:
            ret += '4'
        if blob.tag == 'modify-case':
            ret += '#'
        ret += ' (end-let)'
    elif blob.tag == 'append': # = let $ concat $ ...
        ret = '> (begin-let)\n'
        litstr(blob.attrib['n'])
        ret += '\n$ (var)'
        litstr(blob.attrib['n'])
        ret += '\n$ (var)'
        ret += lines + '\n'
        ret += sint(len(kids)+1) + ' + (concat)\n4 (end-let)'
    elif blob.tag == 'action':
        ret = lines
    elif blob.tag == 'choose':
        for cl in reversed(kids):
            ret = cl + '\nj (jump) ' + slen(ret) + '\n' + ret
    elif blob.tag == 'when':
        ret = ''.join(kids[1:])
        ret = kids[0] + slen(ret) + '\n' + ret
    elif blob.tag == 'otherwise':
        ret = lines
    elif blob.tag in stack:
        ret = lines + '\n' + stack[blob.tag] + slen(kids)
    elif blob.tag == 'b':
        if 'pos' in blob.attrib:
            ret = '_ (blank) ' + sint(''.join([c for c in blob.attrib['pos'] if c.isdigit()]))
        else:
            ret = '  (space)'
    elif blob.tag == 'with-param':
        ret = blob.attrib['pos']
    elif blob.tag == 'call-macro':
        ret = macros[blob.attrib['n']]
        for i,n in enumerate(kids):
            ret = ret.replace('%s.'%sint(i+1), '%s.'%sint(''.join([c for c in n if c.isdigit()])))
    elif blob.tag == 'list':
        litstr(blob.attrib['n'])
    elif blob.tag == 'clip':
        litstr(blob.attrib['part'])
        ret += '\n. (clip) ' + sint(''.join([c for c in blob.attrib['pos'] if c.isdigit()]))
    elif blob.tag == 'var':
        litstr(blob.attrib['n'])
        ret += '\n$ (var)'
    elif blob.tag == 'lit':
        litstr(blob.attrib['v'])
    elif blob.tag == 'lit-tag':
        litstr('<' + blob.attrib['v'].replace('.','><') + '>')
        #ret += '%'
    elif blob.tag == 'pseudolemma':
        ret = kids[0] + '\np (pseudolemma)'
    elif blob.tag == 'case-of':
        litstr(blob.attrib['part'])
        ret += '\n. (clip) ' + sint(''.join([c for c in blob.attrib['pos'] if c.isdigit()])) + '\nG (get-case)'
    return ret

def tostr(blob):
    global listNames, attrNames, catNames, varNames, macros, maxlen
    ret = ""
    kids = [tostr(x) for x in blob]
    if blob.tag == 'pattern':
        maxlen = max(maxlen, len(kids))
    givecount = {'section-def-cats':'C', 'section-def-attrs': 'A', 'def-attr': 'a', 'section-def-vars':'V', 'section-def-lists':'L', 'section-rules':'R', 'pattern':'P'}
    namequote = {'attr-item': 'tags', 'list-item':'v'}
    logic = {'not':'!', 'equal':'=', 'begins-with':'(', 'ends-with':')', 'begins-with-list':'[', 'ends-with-list':']', 'contains-substring':'c', 'in':'n', 'test':'?', 'get-case-from':'G'}
    stack = {'and':'&', 'or':'|', 'concat':'+', 'out':'<', 'chunk':'{'}
    def litstr(s):
        nonlocal ret
        ret += 's' + slen(s) + s
    if blob.tag == 'interchunk':
        ret = sint(maxlen) + ''.join(kids)
    elif blob.tag.startswith('section'):
        if blob.tag == 'section-rules':
            ret = slen(kids) + ''.join(kids)
    elif blob.tag in givecount:
        ret = givecount[blob.tag] + slen(kids) + ''.join(kids)
    elif blob.tag == 'def-cat':
        catNames.append(blob.attrib['n'])
        litstr(blob.attrib['n'])
    elif blob.tag in namequote:
        litstr(blob.attrib[namequote[blob.tag]])
    elif blob.tag == 'def-var':
        varNames.append(blob.attrib['n'])
        if 'v' in blob.attrib:
            ret = '$'
            litstr(blob.attrib['n'])
            litstr(blob.attrib['v'])
        else:
            ret = 'v'
            litstr(blob.attrib['n'])
    elif blob.tag == 'def-list':
        listNames.append(blob.attrib['n'])
        ret = 'l' + slen(kids) + ''.join(kids)
    elif blob.tag == 'def-macro':
        macros[blob.attrib['n']] = ''.join(kids)
    elif blob.tag == 'rule':
        ret = kids[1]
        ret = 'R' + slen(ret) + ret
    elif blob.tag == 'pattern-item':
        ret = sint(catNames.index(blob.attrib['n']))
    elif blob.tag in logic:
        cs = ''
        if blob.tag not in ['not', 'test', 'get-case-from']:
            if 'caseless' in blob.attrib and blob.attrib['caseless'] == 'yes':
                cs = '#'
        ret = ''.join(kids) + logic[blob.tag] + cs
    elif blob.tag in ['let', 'modify-case']:
        ret = '>' + ''.join(kids)
        if blob[0].tag == 'clip':
            ret += '*'
        else:
            ret += '4'
        if blob.tag == 'modify-case':
            ret += '#'
    elif blob.tag == 'append': # = let $ concat $ ...
        ret = '>'
        litstr(blob.attrib['n'])
        ret += '$'
        litstr(blob.attrib['n'])
        ret += '$'
        ret += ''.join(kids)
        ret += sint(len(kids)+1) + '+4'
    elif blob.tag == 'action':
        ret = ''.join(kids)
    elif blob.tag == 'choose':
        for cl in reversed(kids[:-1]):
            ret = cl + 'j' + slen(ret) + ret
        ret += kids[-1]
    elif blob.tag == 'when':
        ret = ''.join(kids[1:])
        ret = kids[0] + slen(ret) + ret
    elif blob.tag == 'otherwise':
        ret = ''.join(kids)
    elif blob.tag in stack:
        ret = ''.join(kids) + stack[blob.tag] + slen(kids)
    elif blob.tag == 'b':
        if 'pos' in blob.attrib:
            ret = '_' + sint(''.join([c for c in blob.attrib['pos'] if c.isdigit()]))
        else:
            ret = ' '
    elif blob.tag == 'with-param':
        ret = blob.attrib['pos']
    elif blob.tag == 'call-macro':
        ret = macros[blob.attrib['n']]
        for i,n in enumerate(kids):
            ret = ret.replace('%s.'%sint(i+1), '%s.'%sint(''.join([c for c in n if c.isdigit()])))
    elif blob.tag == 'list':
        litstr(blob.attrib['n'])
    elif blob.tag == 'clip':
        litstr(blob.attrib['part'])
        ret += '.' + sint(''.join([c for c in blob.attrib['pos'] if c.isdigit()]))
    elif blob.tag == 'var':
        litstr(blob.attrib['n'])
        ret += '$'
    elif blob.tag == 'lit':
        litstr(blob.attrib['v'])
    elif blob.tag == 'lit-tag':
        litstr('<' + blob.attrib['v'].replace('.','><') + '>')
        #ret += '%'
    elif blob.tag == 'pseudolemma':
        ret = kids[0] + 'p'
    elif blob.tag == 'case-of':
        litstr(blob.attrib['part'])
        ret += '.' + sint(''.join([c for c in blob.attrib['pos'] if c.isdigit()])) + 'G'
    if READABLE:
        ret += '\n'
    return ret

rf = sys.argv[1]
of = sys.argv[2]
if len(sys.argv) == 4 and sys.argv[1] == '-r':
    rf = sys.argv[2]
    of = sys.argv[3]
    READABLE = True
rulefile = xml.parse(rf).getroot()
f = open(of, 'w')
if READABLE:
    f.write(toreadstr(rulefile))
else:
    f.write(tostr(rulefile))#.encode('utf-8'))
f.close()
