#!/usr/bin/env python3
import xml.etree.ElementTree as xml
import sys, re

listNames = []
attrNames = []
varNames = []
catNames = []
macros = {}

maxlen = 0
rulecount = 0
currentfile = 1

def str_repl(s, reps):
    # from https://gist.github.com/carlsmith/b2e6ba538ca6f58689b4c18f46fef11c
    subs = sorted(reps, key=len, reverse=True)
    reg = re.compile('|'.join(map(re.escape, subs)))
    return reg.sub(lambda m: reps[m.group(0)], s)

def sint(num):
    if isinstance(num, str):
        num = int(''.join([n for n in num if n.isdigit()]))
    return chr(num)
def slen(ls):
    return sint(len(ls))

ENDOFCHOOSE = False

def tostr(blob):
    global listNames, attrNames, catNames, varNames, macros, maxlen, ENDOFCHOOSE, currentfile, rulecount
    ret = ""
    kids = [tostr(x) for x in blob]
    if blob.tag == 'pattern':
        maxlen = max(maxlen, len(kids))
    namequote = {'attr-item': 'tags', 'list-item':'v'}
    logic = {'not':'!', 'equal':'=', 'begins-with':'(', 'ends-with':')', 'begins-with-list':'[', 'ends-with-list':']', 'contains-substring':'c', 'in':'n', 'test':'?'}
    stack = {'and':'&', 'or':'|', 'concat':'+', 'out':'<', 'chunk':'{'}
    def litstr(s):
        nonlocal ret
        ret += 's' + slen(s) + s
    if blob.tag == 'transfer':
        currentfile = 1
        ret = ''.join(kids)
    if blob.tag == 'interchunk':
        currentfile = 2
        ret = ''.join(kids)
    if blob.tag == 'postchunk':
        currentfile = 3
        ret = ''.join(kids)
    elif blob.tag == 'section-rules':
        ret = ''.join(kids)
    elif blob.tag == 'def-cat':
        catNames.append(blob.attrib['n'])
    elif blob.tag in namequote:
        litstr(blob.attrib[namequote[blob.tag]])
    elif blob.tag == 'def-var':
        varNames.append(blob.attrib['n'])
    elif blob.tag == 'def-list':
        listNames.append(blob.attrib['n'])
    elif blob.tag == 'def-macro':
        macros[blob.attrib['n']] = ''.join(kids)
    elif blob.tag == 'rule':
        ret = kids[1]
        ret = slen(ret) + ret
        rulecount += 1
    elif blob.tag in logic:
        cs = ''
        if blob.tag not in ['not', 'test']:
            if 'caseless' in blob.attrib and blob.attrib['caseless'] == 'yes':
                cs = '#'
        ret = ''.join(kids) + logic[blob.tag] + cs
    elif blob.tag in ['let', 'modify-case']:
        ret = kids[1] + kids[0].replace('T', 't').replace('$', '4')
        if blob.tag == 'modify-case':
            ret += '#'
    elif blob.tag == 'append': # = let $ concat $ ...
        litstr(blob.attrib['n'])
        ret += '$'
        ret += ''.join(kids)
        ret += sint(len(kids)+1) + '+'
        litstr(blob.attrib['n'])
        ret += '4'
    elif blob.tag == 'action':
        ret = ''.join(kids)
    elif blob.tag == 'choose':
        kls = blob.getchildren()
        temp = ENDOFCHOOSE
        ENDOFCHOOSE = True
        ret = tostr(kls[-1])
        ENDOFCHOOSE = False
        for k in reversed(kls[:-1]):
            ret = tostr(k) + 'j' + slen(ret) + ret
        ENDOFCHOOSE = temp
    elif blob.tag == 'when':
        ret = ''.join(kids[1:])
        n = len(ret)
        if not ENDOFCHOOSE:
            n += 2
        ret = kids[0] + sint(n) + ret
    elif blob.tag == 'otherwise':
        ret = ''.join(kids)
    elif blob.tag in stack:
        ret = ''.join(kids) + stack[blob.tag] + slen(kids)
    elif blob.tag == 'b':
        if 'pos' in blob.attrib:
            ret = '_' + sint(blob.attrib['pos'])
        else:
            ret = ' '
    elif blob.tag == 'with-param':
        ret = blob.attrib['pos']
    elif blob.tag == 'call-macro':
        ret = macros[blob.attrib['n']]
        rep = {}
        for i,n in enumerate(kids):
            rep['S%s'%sint(i+1)] = 'S%s'%sint(n)
            rep['T%s'%sint(i+1)] = 'T%s'%sint(n)
            rep['R%s'%sint(i+1)] = 'R%s'%sint(n)
            rep['t%s'%sint(i+1)] = 't%s'%sint(n)
            rep['_%s'%sint(i+1)] = '_%s'%sint(n)
        ret = str_repl(ret, rep)
    elif blob.tag == 'list':
        litstr(blob.attrib['n'])
    elif blob.tag == 'clip':
        litstr(blob.attrib['part'])
        if 'side' in blob.attrib and blob.attrib['side'] == 'sl':
            ret += 'S'
        elif 'side' in blob.attrib and blob.attrib['side'] == 'ref':
            ret += 'R'
        else:
            ret += 'T'
        ret += sint(blob.attrib['pos'])
    elif blob.tag == 'var':
        litstr(blob.attrib['n'])
        ret += '$'
    elif blob.tag == 'lit':
        litstr(blob.attrib['v'])
    elif blob.tag == 'lit-tag':
        s = blob.attrib['v'].replace('.', '><')
        if s:
            s = '<' + s + '>'
        litstr(s)
    elif blob.tag == 'pseudolemma':
        ret = kids[0] + 'p'
    elif blob.tag == 'case-of':
        litstr(blob.attrib['part'])
        ret += 'T' + sint(blob.attrib['pos']) + 'G'
    elif blob.tag == 'get-case-from':
        ret = kids[0]
        litstr("lem")
        ret += 'T' + sint(blob.attrib['pos']) + 'A'
    return ret

r1f = sys.argv[1]
r2f = sys.argv[2]
r3f = sys.argv[3]
of = sys.argv[4]
txt = tostr(xml.parse(r1f).getroot())
txt += tostr(xml.parse(r2f).getroot())
txt += tostr(xml.parse(r3f).getroot())
f = open(of, 'w')
f.write(sint(maxlen)+sint(rulecount)+txt)
f.close()
