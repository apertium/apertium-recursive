#!/usr/bin/python3 
# coding=utf-8
# -*- encoding: utf-8 -*-

# example usage:
# echo "a word" | apertium -d ../apertium-eng-spa eng-spa-lex | python3 glr_parser.py test_rules.rtx

# note: currently will not handle eng-spa.rtx

import sys, re

Attrs = {}
OutRules = {}

class Lexical:
    def __init__(self, s_lem, t_lem, s_tags, t_tags, vrs, matchvars, outform):
        self.s_lem = s_lem
        self.t_lem = t_lem
        self.s_tags = s_tags
        self.t_tags = t_tags
        self.vrs = vrs
        self.matchvars = matchvars
        self.outform = outform
        self.get_attrs()
    def __repr__(self):
        return 'Lexical(%s/%s, %s/%s)' % (self.t_lem, self.t_lem, self.s_tags, self.t_tags)
    s_regex = re.compile('\\^([^<>]+)((?:<[^<>]+>)+)/([^<>]+)((?:<[^<>]+>)+)\\$')
    f_regex = re.compile('(\\w*)@([\\w.$]*)')
    def get_attrs(self):
        for k in Attrs:
            for s in self.s_tags:
                if s in Attrs[k]:
                    self.vrs[k] = s
            for t in self.t_tags:
                if t in Attrs[k]:
                    self.vrs[k] = t
    def fromstream(s):
        p = Lexical.s_regex.match(s)
        sl = p.group(1)
        tl = p.group(3)
        sl_tags = p.group(2)[1:-1].split('><')
        tl_tags = p.group(4)[1:-1].split('><')
        vrs = {}
        out = None
        return Lexical(sl, tl, sl_tags, tl_tags, vrs, [], None)
    def fromfile(s, ispat):
        m = Lexical.f_regex.match(s)
        tags = m.group(2).split('.')
        vrs = {t[1:]:None for t in tags if t[0] == '$'}
        if ispat:
            tags = [t for t in tags if t[0] != '$']
            return Lexical(m.group(1), '', tags, [], {}, vrs, None)
        else:
            out = m.group(1)
            for t in tags:
                if t[0] == '$':
                    out += '<{vrs[%s]}>' % t[1:]
                else:
                    out += '<%s>' % t
            return Lexical('', m.group(1), [], tags, vrs, [], '^'+out+'$')
    def output(self, vrs):
        allvrs = self.vrs.copy()
        allvrs.update(vrs)
        if self.outform:
            out = self.outform
        else:
            out = '[[[ error: no rule found for %s ]]]' % '.'.join(self.t_tags)
            for i in range(len(self.t_tags)):
                s = '.'.join(self.t_tags[:len(self.t_tags)-i])
                if s in OutRules:
                    out = OutRules[s]
        return out.format(lemma=self.t_lem, vrs=allvrs)
    def match(self, data, vrs):
        if not isinstance(data, Lexical):
            return (False, {})
        newvars = {}
        lem = self.s_lem
        if lem and lem[0] == '$':
            if data.s_lem not in Attrs[lem[1:]]:
                return (False, {})
        elif lem and data.s_lem != lem:
            return (False, {})
        for a,b in zip(self.s_tags, data.s_tags):
            if a[0] == '$':
                if a[1:] in vrs:
                    if b != vrs[a[1:]]:
                        return (False, {})
                elif a[1:] in Attrs:
                    if b not in Attrs[a[1:]]:
                        return (False, {})
                else:
                    newvars[a[1:]] = b
            else:
                if a != b:
                    return (False, {})
        return (True, newvars)
            
class Blank:
    def __init__(self, text):
        self.text = text
    def output(self, vrs):
        return self.text
    def match(self, data, vrs):
        return (True, {})
    def __repr__(self):
        return 'Blank(%s)' % (self.text)
class Syntax:
    def __init__(self, ntype, vrs, outrule, children, updates):
        self.ntype = ntype
        self.vrs = vrs
        self.outrule = outrule
        self.children = children
        self.updates = updates
    def __repr__(self):
        return 'Syntax(%s, %s)' % (self.ntype, self.children)
    def output(self, vrs):
        allvrs = vrs
        allvrs.update(self.vrs)
        for s_id, s_var, d_id, d_var in self.updates:
            if s_id == None:
                val = allvrs[s_var]
            elif s_id == 'lit':
                val = s_var
            else:
                val = self.children[s_id].vrs[s_var]
            if d_id == 'self':
                allvrs[d_var] = val
            else:
                self.children[d_id].vrs[d_var] = val
        ls = [x.output(allvrs) for x in self.children]
        return self.outrule.format(*ls, _=' ', vrs=allvrs)
    def match(self, data, vrs):
        if not isinstance(data, Syntax):
            return (False, {})
        if self.ntype != data.ntype:
            return (False, {})
        newvars = {}
        for v in self.vrs:
            if v in vrs:
                if data.vrs[v] != vrs[v]:
                    return (False, {})
            else:
                newvars[v] = data.vrs[v]
        return (True, newvars)

class Rule:
    all_rules = []
    def __init__(self, ntype, vrs, weight, pattern, output, updates, grab):
        self.ntype = ntype
        self.vrs = vrs
        self.weight = weight
        self.pattern = [pattern[0]]
        for p in pattern[1:]:
            self.pattern.append(Blank(''))
            self.pattern.append(p)
        self.output = output
        self.updates = updates
        self.grab = grab
        Rule.all_rules.append(self)
    def apply(self, tokens):
        if len(self.pattern) > len(tokens):
            return None
        vrs = {x:None for x in self.vrs}
        for p,d in zip(reversed(self.pattern), reversed(tokens)):
            m = p.match(d, vrs)
            if m[0]:
                vrs.update(m[1])
            else:
                return None
        ret = tokens[:-len(self.pattern)]
        for idx, var in self.grab:
            vrs[var] = tokens[idx-len(self.pattern)].vrs[var]
        ret.append(Syntax(self.ntype, vrs, self.output,
                          tokens[-len(self.pattern):], self.updates))
        return (ret, self.weight)
    def apply_all(parses):
        todo = parses
        done = []
        gotany = True
        while gotany:
            gotany = False
            for tk, w in todo:
                localany = False
                for rl in Rule.all_rules:
                    a = rl.apply(tk)
                    if a:
                        gotany = True
                        localany = True
                        done.append((a[0], w+a[1]))
                if not localany:
                    done.append((tk, w))
            todo = done
            done = []
        return todo

def input_stream(stream):
    ret = []
    islex = False
    cur = ''
    for s in stream:#stream.read():
        if not islex and s == '^':
            ret.append(Blank(cur))
            cur = s
            islex = True
        elif islex and s == '$':
            cur += s
            ret.append(Lexical.fromstream(cur))
            cur = ''
            islex = False
        else:
            cur += s
    if cur:
        ret.append(Blank(cur))
    return ret
def parse_file(fname):
    f = open(fname)
    s = re.sub('![^\n]*', '', f.read(), flags=re.MULTILINE).strip()
    f.close()
    rule_regex = re.compile('([\\w.]+)\\s*(:|=|->)\\s*(.*)', re.MULTILINE | re.DOTALL)
    line_regex = re.compile('\\s*([0-9.]+):\\s*([^{}]+)\\{([^{}]+)\\}\\s*([;|])(.*)', re.MULTILINE | re.DOTALL)
    out_regex = re.compile('(_\\d*|\\w*@[\\w.$]*|\\d+(?:\\([\\w=.,\\s]+\\))?)', re.MULTILINE)        
    while s:
        m = rule_regex.match(s)
        if not m:
            print(s)
        name = m.group(1)
        mode = m.group(2)
        s = m.group(3)
        if mode == '=':
            at, s = s.split(';', 1)
            s = s.lstrip()
            Attrs[name] = at.split()
        elif mode == ':':
            pat, s = s.split(';', 1)
            s = s.lstrip()
            pls = name.strip().split('.')
            sls = pat.strip().split('.')
            _ = ''.join('<'+x+'>' for x in pls)
            rl = '^{lemma}'
            for x in sls:
                if x == '_':
                    rl += _
                else:
                    rl += '<{vrs['+x+']}>'
            OutRules[name] = rl + '$'
        else:
            ntype = name.split('.')[0]
            vrs = name.split('.')[1:]
            while s:
                m = line_regex.match(s)
                weight = float(m.group(1))
                s = m.group(5)
                pat = []
                grab = []
                for idx, p in enumerate(m.group(2).split()):
                    if p[0] == '%':
                        for v in vrs:
                            grab.append((idx*2, v))
                        p = p[1:]
                    if '@' in p:
                        pat.append(Lexical.fromfile(p, True))
                    else:
                        l = p.split('.$')
                        pat.append(Syntax(l[0], l[1:], None, [], None))
                res = ''
                updates = []
                ls = out_regex.findall(m.group(3))
                for r in out_regex.findall(m.group(3)):
                    if not isinstance(r, str):
                        r = r[0]
                    if not r:
                        continue
                    if '@' in r:
                        res += Lexical.fromfile(r, False).outform
                    elif r == '_':
                        res += ' '
                    elif r[0] == '_':
                        res += '{%s}' % ((int(r[1:]) * 2) - 1)
                    elif r.isdigit():
                        res += '{%s}' % ((int(r) * 2) - 2)
                    else:
                        loc = (int(r.split('(')[0]) * 2) - 2
                        res += '{%s}' % loc
                        for up in r[:-1].split('(')[1].split(','):
                            var,val = up.split('=')
                            var = var.strip()
                            val = val.strip()
                            if '.' in val:
                                a,b = val.split('.')
                                updates.append(((int(a)*2)-2, b, loc, var))
                            elif val[0] == '$':
                                updates.append((None, val[1:], loc, var))
                            else:
                                updates.append(('lit', val, loc, var))
                Rule(ntype, vrs, weight, pat, res, updates, grab)
                if m.group(4) == ';':
                    break
            s = s[1:].lstrip()

if __name__ == '__main__':
    parse_file(sys.argv[1])
    #tk = input_stream(sys.stdin)
    text = "^a<det><ind><sg>/uno<det><ind><GD><sg>$ ^word<n><sg>/palabra<n><f><sg>$^.<sent>/.<sent>$"
    tk = input_stream(text)
    add = []
    cur = []
    for t in tk:
        if isinstance(t, Blank):
            cur.append(t)
        else:
            cur.append(t)
            add.append(cur)
            cur = []
    parses = [([], 0)]
    for a in add:
        parses = Rule.apply_all(parses)
        parses = [(x[0]+a, x[1]) for x in parses]
    out = min(parses, key=lambda p: p[1] + len(p[0]))[0]
    print(''.join(x.output({}) for x in out))
