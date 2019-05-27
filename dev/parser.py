#!/usr/bin/python3 
# coding=utf-8
# -*- encoding: utf-8 -*-

import os.path
# from https://stackoverflow.com/questions/4060221/how-to-reliably-open-a-file-in-the-same-directory-as-a-python-script
__location__ = os.path.realpath(os.path.join(os.getcwd(), os.path.dirname(__file__)))
def read_template(fname):
    f = open(os.path.join(__location__, fname))
    data = f.read()
    f.close()
    return data

import re
from itertools import product

ATTRS = {}

class Node:
    node_id = 0
    All_Nodes = {}
    def __init__(self, name=None, param=None):
        self.id = Node.node_id
        Node.node_id += 1
        self.name = name or '__INTERNAL_SYMBOL_%s' % self.id
        Node.All_Nodes[self.name] = self
        self.param = param
    def to_lex(self):
        return ''
    def to_yacc(self):
        return ''
    def to_yacc_symbol(self):
        return ''
    def get(name):
        return Node.All_Nodes[name]
    def getname(self):
        if self.param:
            return [self.name + '_' + x for x in ATTRS[self.param]]
        else:
            return [self.name]

class Structural(Node):
    def __init__(self, opts, **kwargs):
        Node.__init__(self, **kwargs)
        self.opts = opts
    def to_yacc_symbol(self):
        #return '%type <non_term> ' + self.name
        return '\n'.join('%type <non_term> ' + x for x in self.getname())
    def to_yacc(self):
        ret = []
        for idx, name in enumerate(self.getname()):
            olines = []
            frame = 'new node_pair("{0}", %s, "{0}", %s, %s);'.format(name)
            for src, dest in self.opts:
                outls = []
                for i, p in enumerate(src):
                    n = Node.get(p)
                    if isinstance(n, Structural):
                        outls.append('$' + str(i+1))
                    else:
                        if i+1 in dest:
                            app = 'new node_pair("{0}", ${1}, "{0}", ${1})'
                        else:
                            app = 'new node_pair("{0}", ${1}, "sl")'
                        outls.append(app.format(p, i+1))
                for d in dest:
                    if isinstance(d, Lexical):
                        outls.append(d.to_sub_yacc())
                insert_index = len(src) + 1
                for i, d in enumerate(dest):
                    if isinstance(d, int):
                        outls.append(str(d))
                    else:
                        outls.append(str(insert_index))
                        insert_index += 1
                psrc = []
                for s in src:
                    n = Node.get(s)
                    if n.param == self.param:
                        psrc.append(n.getname()[idx])
                    else:
                        psrc.append(s)
                output = frame % (len(src), len(dest), ', '.join(outls))
                olines.append('%s { $$ = %s }' % (' '.join(psrc), output))
            ret.append('\n%s:\n    %s\n    ;\n' % (name, '\n    | '.join(olines)))
        return '\n'.join(ret)

class Lexical(Node):
    def __init__(self, lem, tags, isres=False, **kwargs):
        Node.__init__(self, **kwargs)
        self.lem = lem
        self.tags = tags
        self.printed = isres # don't give output tokens to lex
    def nodup(lem, tags, isres=False):
        for k in Node.All_Nodes:
            n = Node.All_Nodes[k]
            if isinstance(n, Lexical) and n.lem == lem and n.tags == tags:
                return n
        return Lexical(lem, tags, isres=isres)
    def to_sub_yacc(self):
        return 'new node_pair("%s", "%s", "tl")' % (self.tags[0], self.lem)
    def to_yacc_symbol(self):
        if self.printed:
            return ''
        else:
            self.printed = True
        return '\n'.join('%token <term> ' + n for n in self.getname())
    def to_lex(self, altname=None):
        if self.printed:
            return ''
        else:
            self.printed = True
        if altname:
            real_name = self.name
            self.name = altname
        ret = []
        for idx, name in enumerate(self.getname()):
            lm = re.escape(self.lem) or '[^<\\$]+'
            tg = []
            for t in self.tags:
                if t == '*':
                    tg.append(['(<[^>]+>)*'])
                else:
                    if isinstance(t, str):
                        l = [t]
                    elif isinstance(t, list):
                        l = t
                    else: # a set() representing a param
                        l = ATTRS[list(t)[0]][idx]
                    tg.append(['<' + re.escape(x) + '>' for x in l])
            s = '\\^%s%%s\\/[^<\\$]*(<[^>]+>)*\\$ { ok; return %s; }' % (lm, name)
            ret.append('\n'.join(s % ''.join(x) for x in product(*tg)))
        if altname:
            self.name = real_name
        return '\n'.join(ret)

class LexCat(Node):
    def __init__(self, opts, **kwargs):
        Node.__init__(self, **kwargs)
        self.opts = opts
    def to_yacc_symbol(self):
        no_extras = [x.to_yacc_symbol() for x in self.opts]
        return '\n'.join('%token <term> ' + n for n in self.getname())
    def to_lex(self):
        return '\n'.join(x.to_lex(altname=self.name) for x in self.opts)

def parse_grammar(in_str):
    com = False
    esc = False
    s = ''
    for c in in_str:
        if com:
            if c == '\n':
                com = False
                s += c
        elif esc:
            s += c
            esc = False
        else:
            if c == '\\':
                esc = True
                s += c
            elif c == '!':
                com = True
            else:
                s += c
    def parse_pattern():
        nonlocal s
        ret = []
        p = re.compile('^\s*([A-Za-z0-9_]+)\s*(.*)$', re.MULTILINE | re.DOTALL)
        while s and s[0] != '{':
            if s[0] == '#':
                l = parse_lemma()
                ret.append(l.name)
            else:
                m = p.match(s)
                ret.append(m.group(1))
                s = m.group(2)
        return ret
    def parse_lemma(isres=False):
        nonlocal s
        m = re.match('^#((?:\\\\.|[^()\s])*)\s*(.*)$', s, re.MULTILINE | re.DOTALL)
        lem = m.group(1)
        s = m.group(2)
        if not s or s[0] != '(':
            return Lexical(lem, [], isres=isres)
        tags = []
        tagpat = re.compile('^([@A-Za-z0-9_-]+|\\[[@A-Za-z0-9_ -]+\\]|\\<[A-Za-z0-9_]+\\>|\\$[A-Za-z0-9_]+|\\*)\\.?(.*)$', re.MULTILINE | re.DOTALL)
        s = s[1:]
        param = None
        while s and s[0] != ')':
            m = tagpat.match(s)
            s = m.group(2)
            if m.group(1)[0] == '[':
                tags.append(m.group(1)[1:-1].split())
            elif m.group(1)[0] == '<':
                tags.append(ATTRS[m.group(1)[1:-1]])
            elif m.group(1)[0] == '$':
                tags.append(set([m.group(1)[1:]]))
                param = m.group(1)[1:]
            else:
                tags.append(m.group(1))
        s = s[1:].lstrip()
        return Lexical(lem, tags, isres=isres, param=param)
    def parse_result():
        nonlocal s
        assert(s[0] == '{')
        numpat = re.compile('^\\s*([1-9][0-9]*)\\s*(.*)$', re.MULTILINE | re.DOTALL)
        s = s[1:].lstrip()
        ret = []
        while s and s[0] != '}':
            m = numpat.match(s)
            if m:
                ret.append(int(m.group(1)))
                s = m.group(2)
            else:
                ret.append(parse_lemma(isres=True))
        s = s[1:].lstrip()
        return ret
    def parse_attr():
        nonlocal s
        m = re.match('^\s*([A-Za-z0-9_]+)\s*=\s*([@A-Za-z0-9_ -]+)\s*;(.*)$', s, re.MULTILINE | re.DOTALL)
        if not m: print(s)
        ATTRS[m.group(1)] = m.group(2).strip().split()
        s = m.group(3).lstrip()
    def parse_rule():
        nonlocal s
        m = re.match('^\s*(\\#?)([A-Za-z0-9_]+)( \\$[A-Za-z0-9_]+|)\s*(?:->|→)\s*(.*)$', s, re.MULTILINE | re.DOTALL)
        if not m:
            parse_attr()
            return
        name = m.group(2)
        param = m.group(3)
        if param:
            param = param[2:]
        s = m.group(4)
        opts = []
        lex = m.group(1)
        while s and s[0] != ';':
            if lex:
                opts.append(parse_lemma())
            else:
                opts.append([parse_pattern(), parse_result()])
            if s and s[0] == '|':
                s = s[1:].lstrip()
        s = s[1:].lstrip()
        if lex:
            return LexCat(opts, name=name, param=param)
        else:
            return Structural(opts, name=name, param=param)
    while s:
        parse_rule()

def make_lex():
    pats = [x.to_lex() for x in Node.All_Nodes.values() if isinstance(x, LexCat)]
    for l in Node.All_Nodes.values():
        if isinstance(l, Lexical) and not l.printed:
            pats.append(l.to_lex())
    s = read_template('template.l')
    s = s.replace('PYTHON_REPLACEMENT_BLOCK', '\n'.join(pats))
    return s

def make_yacc():
    symbs = [x.to_yacc_symbol() for x in Node.All_Nodes.values() if isinstance(x, Structural) or isinstance(x, LexCat)]
    for l in Node.All_Nodes.values():
        if isinstance(l, Lexical) and not l.printed:
            symbs.append(l.to_yacc_symbol())
    rules = ''.join(x.to_yacc() for x in Node.All_Nodes.values())
    regexs = '''
    attr_regexps["lem"] = "(([^<]|\\"\\\\<\\")+)";
    attr_regexps["whole"] = "(.+)";
    attr_regexps["tags"] = "((<[^>]+>)+)";
    attr_regexps["func"] = "(<@[^<]+>)";
''' #TODO: What are these regexs doing?
    s = read_template('template.y')
    s = s.replace('PYTHON_REPLACEMENT_SPOT1', '\n'.join(symbs))
    s = s.replace('PYTHON_REPLACEMENT_SPOT2', rules)
    s = s.replace('PYTHON_REPLACEMENT_SPOT3', regexs)
    return s

if __name__ == '__main__':
    import sys
    s = sys.stdin.read()
    parse_grammar(s)
    if sys.argv[1] == '-l':
        print(make_lex())
    else:
        print(make_yacc())
