#!/usr/bin/env python3
import xml.etree.ElementTree as xml
import sys

listNames = []
attrNames = []
varNames = []
catNames = []
macros = {}

TODO = ['modify-case', 'get-case-from', 'case-of', 'pseudolemma', 'append']

maxlen = 0

def tostr(blob):
    global listNames, attrNames, catNames, varNames, macros, maxlen
    ret = ""
    kids = [tostr(x) for x in blob]
    if blob.tag == 'pattern':
        maxlen = max(maxlen, len(kids))
    givecount = {'section-def-cats':'C', 'section-def-attrs': 'A', 'def-attr': 'a', 'section-def-vars':'V', 'section-def-lists':'L', 'section-rules':'R', 'pattern':'P'}
    namequote = {'attr-item': 'tags', 'list-item':'v'}
    logic = {'not':'!', 'equal':'=', 'begins-with':'(', 'ends-with':')', 'begins-with-list':'[', 'ends-with-list':']', 'contains-substring':'c', 'in':'n', 'test':'?'}
    stack = {'and':'&', 'or':'|', 'concat':'+', 'out':'<', 'chunk':'{'}
    def litstr(s):
        nonlocal ret
        ret += 's' + chr(len(s)) + s
    if blob.tag == 'interchunk':
        ret = chr(len(kids)) + chr(maxlen) + ''.join(kids)
    elif blob.tag.startswith('section'):
        if blob.tag == 'section-rules':
            ret = ''.join(kids)
    elif blob.tag in givecount:
        ret = givecount[blob.tag] + chr(len(kids)) + ''.join(kids)
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
        ret = 'l' + chr(len(kids)) + ''.join(kids)
    elif blob.tag == 'def-macro':
        macros[blob.attrib['n']] = ''.join(kids)
    elif blob.tag == 'rule':
        ret = kids[1]
        ret = 'R' + chr(len(ret)) + ret
    elif blob.tag == 'pattern-item':
        ret = chr(catNames.index(blob.attrib['n']))
    elif blob.tag in logic:
        cs = ''
        if blob.tag not in ['not', 'test']:
            if 'caseless' in blob.attrib and blob.attrib['caseless'] == 'yes':
                cs = '#'
        ret = ''.join(kids) + logic[blob.tag] + cs
    elif blob.tag == 'let':
        ret = '>' + ''.join(kids)
        if blob[0].tag == 'clip':
            ret += '*'
        else:
            ret += '4'
    elif blob.tag == 'action':
        ret = ''.join(kids)
    elif blob.tag == 'choose':
        ret = ''.join(kids)
        ret = '~' + chr(len(ret)) + ret
    elif blob.tag == 'when':
        ret = ''.join(kids[1:])
        ret = 'i' + chr(len(ret)) + kids[0] + ret
    elif blob.tag == 'otherwise':
        ret = 'e' + ''.join(kids)
    elif blob.tag in stack:
        ret = ''.join(kids) + stack[blob.tag] + chr(len(kids))
    elif blob.tag == 'b':
        if 'pos' in blob.attrib:
            ret = '_' + blob.attrib['pos']
        else:
            ret = ' '
    elif blob.tag == 'with-param':
        ret = blob.attrib['pos']
    elif blob.tag == 'call-macro':
        ret = macros[blob.attrib['n']]
        for i,n in enumerate(kids):
            ret = ret.replace('%s.'%chr(i+1), '%s.'%chr(int(n)))
    elif blob.tag == 'list':
        litstr(blob.attrib['n'])
    elif blob.tag == 'clip':
        litstr(blob.attrib['part'])
        ret += '.' + chr(int(blob.attrib['pos']))
    elif blob.tag == 'var':
        litstr(blob.attrib['n'])
        ret += '$'
    elif blob.tag == 'lit':
        litstr(blob.attrib['v'])
    elif blob.tag == 'lit-tag':
        litstr('<' + blob.attrib['v'].replace('.','><') + '>')
        ret += '%'
    return ret

rulefile = xml.parse(sys.argv[1]).getroot()
f = open(sys.argv[2], 'w')
f.write(tostr(rulefile))#.encode('utf-8'))
f.close()
