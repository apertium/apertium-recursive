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
    if READABLE:
        return '[%s]' % num
    else:
        return chr(num)
def slen(ls):
    return sint(len(ls))

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
        for cl in reversed(kids):
            ret = cl + 'j' + slen(ret) + ret
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
    return ret

rf = sys.argv[1]
of = sys.argv[2]
if len(sys.argv) == 4 and sys.argv[1] == '-r':
    rf = sys.argv[2]
    of = sys.argv[3]
    READABLE = True
rulefile = xml.parse(rf).getroot()
f = open(of, 'w')
f.write(tostr(rulefile))#.encode('utf-8'))
f.close()
