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

class Node:
    node_id = 0
    All_Nodes = {}
    def __init__(self, name=None):
        self.id = Node.node_id
        Node.node_id += 1
        self.name = name or '__INTERNAL_SYMBOL_%s' % self.id
        Node.All_Nodes[self.name] = self
    def to_lex(self):
        return ''
    def to_yacc(self):
        return ''
    def to_yacc_symbol(self):
        return ''
    def get(name):
        return Node.All_Nodes[name]

class Structural(Node):
    def __init__(self, opts, **kwargs):
        Node.__init__(self, **kwargs)
        self.opts = opts
    def to_yacc_symbol(self):
        return '%type <non_term> ' + self.name
    def to_yacc(self):
        olines = []
        frame = 'new node_pair("{0}", %s, "{0}", %s, %s);'.format(self.name)
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
            output = frame % (len(src), len(dest), ', '.join(outls))
            olines.append('%s { $$ = %s }' % (' '.join(src), output))
        return '\n%s:\n    %s\n    ;\n' % (self.name, '\n    | '.join(olines))

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
        return '%token <term> ' + self.name
    def to_lex(self, altname=None):
        if self.printed:
            return ''
        else:
            self.printed = True
        l = re.escape(self.lem) or '[^<\\$]+'
        tg = []
        for t in self.tags:
            if t == '*':
                tg.append('(<[^>]+>)*')
            else:
                tg.append('<' + re.escape(t) + '>')
        return '\\^%s%s\\/[^<\\$]*(<[^>]+>)*\\$ { ok; return %s; }' % (l, ''.join(tg), altname or self.name)

class LexCat(Node):
    def __init__(self, opts, **kwargs):
        Node.__init__(self, **kwargs)
        self.opts = opts
    def to_yacc_symbol(self):
        no_extras = [x.to_yacc_symbol() for x in self.opts]
        return '%token <term> ' + self.name
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
        m = re.match('^#((?:\\.|[^()\s])*)(\\((?:[@A-Za-z0-9_-]*\\.)*(?:[@A-Za-z0-9_-]*|\\*)\\))\\s*(.*)$', s, re.MULTILINE | re.DOTALL)
        s = m.group(3)
        tags = m.group(2)
        if tags:
            return Lexical.nodup(m.group(1), tags[1:-1].split('.'), isres=isres)
        else:
            return Lexical.nodup(m.group(1), [], isres=isres)
    def parse_result():
        nonlocal s
        assert(s[0] == '{')
        numpat = re.compile('^\\s*\\$([1-9][0-9]*)\\s*(.*)$', re.MULTILINE | re.DOTALL)
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
    def parse_rule():
        nonlocal s
        m = re.match('^\s*(\\#?)([A-Za-z0-9_]+)\s*(?:->|→)\s*(.*)$', s, re.MULTILINE | re.DOTALL)
        name = m.group(2)
        s = m.group(3)
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
            return LexCat(opts, name=name)
        else:
            return Structural(opts, name=name)

    ret = []
    while s:
        ret.append(parse_rule())
    return ret

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
