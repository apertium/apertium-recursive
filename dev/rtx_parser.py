#!/usr/bin/env python3

import sys, re

text = ""
tokens = []

SPECIAL = list("@$%()={}[]|\\/:;<>,.") + ['->']

Attributes = {}
AttrInverse = {}

AllSymbols = ['UNKNOWN', '_', 'GLUE']

def getIdent():
    global tokens
    if not tokens:
        raise SyntaxError("Unexpected end of file")
    tok = tokens.pop(0)
    if tok in SPECIAL:
        raise SyntaxError("Expected identifier, found %s" % tok)
    return tok
    
def getTok(shouldbe = []):
    global tokens
    if not tokens:
        raise SyntaxError("Unexpected end of file")
    tok = tokens.pop(0)
    if shouldbe and tok not in shouldbe:
        raise SyntaxError("Expected one of these: %s, got %s" % (shouldbe, tok))
    return tok

class ReduceRule:
    AllRules = []
    def __init__(self, result, grabs, pattern, updates, output, weight):
        self.result = result
        self.grabs = grabs
        self.pattern = pattern
        self.updates = updates
        self.output = output
        self.id = len(ReduceRule.AllRules)
        self.weight = weight
        self.cond = ''
        self.rule = ''
        self.act = ''
        ReduceRule.AllRules.append(self)
    def process(self):
        global AllSymbols
        if self.result not in AllSymbols:
            AllSymbols.append(self.result)
        pat = []
        cnd = []
        for i, p in enumerate(self.pattern):
            sym = p[0]
            rest = p[1:]
            if sym == '@':
                sym += p[1]
                rest = p[2:]
            if sym not in AllSymbols:
                AllSymbols.append(sym)
            pat.append(sym)
            n = 2*i + 1
            for s in rest:
                c = []
                for v in AttrInverse[s]:
                    c.append('$%s->getVar(L"%s") == L"%s"' % (n, v, s))
                cnd.append('(' + ' || '.join(c) + ')')
        if cnd:
            self.cond = '%?{ ' + ' && '.join(cnd) + ' } '
        setup = ['vector<ParserNode*> NODESETUP;']
        for n in range(1, 2*len(self.pattern)):
            setup.append('NODESETUP.push_back($%s);' % n)
        setup.append('$$ = new ParserNode(NODESETUP, %s);' % self.id)
        for i, v in self.grabs:
            setup.append('$$->setVar(L"%s", $%s->getVar(L"%s"));' % (v, 2*i+1, v))
        self.rule = '%s %s %%dprec %s { %s }' % (self.cond, ' _ '.join(pat), self.weight, ' '.join(setup))
        up = []
        for dest, vdest, src, vsrc in self.updates:
            nd = (dest-1)*2
            ns = (src-1)*2
            if src == -1:
                val = 'L"%s"' % vsrc
            elif src == 0:
                val = 'getVar(L"%s")' % vsrc
            else:
                val = 'children[%s]->getVar(L"%s")' % (ns, vsrc)
            up.append('children[%s]->setVar(L"%s", %s);' % (nd, vdest, val))
        out = []
        for o in self.output:
            if o[0] == '_':
                out.append('children[%s]->output();' % ((int(o[1:])-1)*2 + 1))
            else:
                out.append('children[%s]->output();' % ((int(o)-1)*2))
        self.act = 'case %s: %s %s break;' % (self.id, ' '.join(up), ' '.join(out))
    def fromstring():
        rules = []
        ntype = tokens.pop(0)
        grabvars = []
        while tokens[0] == '.':
            getTok()
            grabvars.append(getIdent())
        getTok("->")
        while True:
            weight = int(getTok())
            getTok(":")
            curgrabs = []
            patels = []
            while tokens and tokens[0] != '{':
                cur = []
                if tokens[0] == '@':
                    cur.append(getTok())
                cur.append(getIdent())
                while tokens and tokens[0] == '.':
                    getTok()
                    t = getTok()
                    if t == '$':
                        curgrabs.append([len(patels), getIdent()])
                    else:
                        cur.append(t)
                patels.append(cur)
                #@TODO: lemmas
            getTok('{')
            outels = []
            updates = []
            while tokens and tokens[0] != '}':
                outels.append(getIdent())
                if tokens[0] == '(':
                    getTok()
                    while tokens[0] != ')':
                        v1 = getIdent()
                        getTok('=')
                        src = getTok()
                        if src == '$':
                            updates.append([int(outels[-1]), v1, 0, getIdent()])
                        elif src.isdigit():
                            getTok('.')
                            updates.append([int(outels[-1]), v1, int(src), getIdent()])
                        else:
                            updates.append([int(outels[-1]), v1, -1, src])
                        if tokens[0] == ',':
                            getTok()
                    getTok()
            getTok('}')
            rules.append(ReduceRule(ntype, curgrabs, patels, updates, outels, weight))
            if getTok(['|', ';']) == ';':
                break
        return rules

OutputRules = []

def parseOutputRule():
    pat = [getIdent()]
    while tokens[0] == '.':
        getTok()
        pat.append(getIdent())
    getTok(':')
    out = []
    while True:
        if tokens[0] == '<':
            out.append(getTok() + getIdent() + getTok('>'))
        else:
            out.append(getIdent())
        if getTok([';', '.']) == ';':
            break
    global OutputRules
    #OutputRules.append([pat, out])
    cond = ['nodeType == L"%s"' % pat[0]]
    for p in pat[1:]:
        l = []
        for cat in AttrInverse[p]:
            l.append('getVar(L"%s") == %s' % (cat, p))
        cond.append('(' + ' || '.join(l) + ')')
    prt = []
    for o in out:
        if o == '_':
            prt.append('wcout << getVar(L"lemma");')
            for p in pat:
                prt.append('wcout << L"<%s>";' % p)
        elif o[0] == '<':
            prt.append('wcout << L"%s";' % o)
        else:
            prt.append('if(getVar(L"%s")!=L""){ wcout << L"<" << getVar(L"%s") << L">"; }' % (o, o))
    OutputRules.append('if(%s){ %s }' % (' && '.join(cond), ' '.join(prt)))

Retags = []

def parseRetagRule():
    global Retags
    src = [getIdent()]
    while tokens[0] == '.':
        getTok()
        src.append(getIdent())
    getTok('>')
    dest = [getIdent()]
    while tokens[0] == '.':
        getTok()
        dest.append(getIdent())
    getTok(':')
    pairs = []
    while True:
        s = [getIdent()]
        while tokens[0] == '.':
            getTok()
            s.append(getIdent())
        d = [getIdent()]
        while tokens[0] == '.':
            getTok()
            d.append(getIdent())
        pairs.append([s,d])
        if getTok([',', ';']) == ';':
            break
    Retags.append([src, dest, pairs])

def parseRule():
    for t in tokens:
        if t == ':':
            parseOutputRule()
            break
        elif t == '>':
            parseRetagRule()
            break
        elif t == '=':
            cat = getIdent()
            getTok('=')
            at = []
            while tokens[0] != ';':
                at.append(getIdent())
            global Attributes
            assert cat not in Attributes
            Attributes[cat] = at
            global AttrInverse
            for a in at:
                if a not in AttrInverse:
                    AttrInverse[a] = []
                AttrInverse[a].append(cat)
            getTok(';')
            break
        elif t == '->':
            ReduceRule.fromstring()
            break

def parse(fname):
    f = open(fname)
    global text
    text = re.sub('(?<!\\\\)![^\n]*', ' ', f.read(), flags=re.MULTILINE).strip()
    global tokens
    tokens = [tok for tok in re.split('\\s+|(->|[@$%()={}\\[\\]|\\\\/:;<>,.])', text) if tok]
    f.close()
    while tokens:
        parseRule()
        
if __name__ == '__main__':
    from collections import defaultdict as dd
    rls = dd(list)
    parse(sys.argv[1])
    for r in ReduceRule.AllRules:
        r.process()
        rls[r.result].append(r.rule)
    for s in AllSymbols:
        if s == 'GLUE' or s == '_': continue
        print(s)
        l = [s]
        if s[0] == '@':
            l = [s[0], s[1:]]
        x = ReduceRule('GLUE', [], [l], [], ['1'], 1)
        x.process()
        rls['GLUE'].append(x.rule)
        x = ReduceRule('GLUE', [], [['GLUE'], l], [], ['1', '_1', '2'], 1)
        x.process()
        rls['GLUE'].append(x.rule)
    tk = []
    tk2 = []
    for s in AllSymbols:
        if s in ['_', 'UNKNOWN'] or s[0] == '@':
            tk.append('%token <node> ' + s)
            tk2.append('AllTokens.insert(pair<wstring, int>(L"%s", %s));' % (s,s))
        else:
            tk.append('%type <node> ' + s)
    rltxt = []
    for k in rls:
        rltxt.append('%s : %s \n ;' % (k, '\n  |  '.join(rls[k])))
    f = open('/home/daniel/apertium-recursive/src/bison_template.y')
    txt = f.read()
    f.close()
    txt = txt.replace('@@TOKENS@@', '\n'.join(tk))
    txt = txt.replace('@@TOKENS2@@', '\n'.join(tk2))
    txt = txt.replace('@@TREES@@', '\n\n'.join(rltxt))
    txt = txt.replace('@@LEXOUTPUT@@', '\n    else '.join(OutputRules))
    txt = txt.replace('@@NODEOUTPUT@@', '\n      '.join(x.act for x in ReduceRule.AllRules))
    txt = txt.replace('@', '__at__')
    f = open(sys.argv[2], 'w')
    f.write(txt)
    f.close()
