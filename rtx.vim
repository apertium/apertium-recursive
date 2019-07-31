" Vim syntax file
" Language:	Apertium Recursive Transfer Ruleset
" Filenames:    *_rtx
" Maintainer:	Daniel Swanson <awesomeevilddudes@gmail.com>
" Last Change:	2019 Jul 22

" Quit when a (custom) syntax file was already loaded
if exists("b:current_syntax")
  finish
endif

syn region	rtxComment		start="!" end="\n" keepend contains=rtxComment
syn region	rtxString		start=+"+ end=+"+ skip=+\\"+
syn match	rtxChunkTag		"\v\$[^\. ()=]+"
syn match	rtxWeight		"\v\d+(\.\d+)?:"
syn match	rtxAll			"%"
syn match       rtxOp                   "\v\c<[-_]*a[-_]*n[-_]*d[-_]*>"
syn match       rtxOp                   "\v\c<[-_]*o[-_]*r[-_]*>"
syn match       rtxOp                   "\v\c<[-_]*n[-_]*o[-_]*t[-_]*>"
syn match       rtxOp                   "\v[-_]*[|~⌐&=;][-_]*"
syn match       rtxOp                   "\v\c<[-_]*((i|h[-_]*a)[-_]*s[-_]*((p[-_]*r[-_]*e|s[-_]*u[-_]*f)[-_]*f[-_]*i[-_]*x|s[-_]*u[-_]*b[-_]*s[-_]*t[-_]*r[-_]*i[-_]*n[-_]*g)|(s[-_]*t[-_]*a[-_]*r[-_]*t|b[-_]*e[-_]*g[-_]*i[-_]*n|e[-_]*n[-_]*d)[-_]*s[-_]*w[-_]*i[-_]*t[-_]*h(l[-_]*i[-_]*s[-_]*t)?|i[-_]*n|∈|e[-_]*q[-_]*u[-_]*a[-_]*l)([-_]*(c[-_]*l|c[-_]*a[-_]*s[-_]*e[-_]*l[-_]*e[-_]*s[-_]*s|f[-_]*o[-_]*l[-_]*d([-_]*c[-_]*a[-_]*s[-_]*e)?))?[-_]*>"
syn match       rtxIfKeyword            "\v\c<[-_]*(i[-_]*f|e[-_]*l[-_]*s[-_]*e([-_]*i[-_]*f)?|a[-_]*l[-_]*w[-_]*a[-_]*y[-_]*s)[-_]*>"
syn match       rtxOutput               "\v<_?\d*>"
syn match       rtxMacroCall            "\v\[\w+\]"

" Define the default highlighting.
" Only used when an item doesn't have highlighting yet

hi def link rtxComment			Comment
hi def link rtxIfKeyword		Keyword
hi def link rtxChunkTag			Identifier
hi def link rtxString			String
hi def link rtxWeight			Constant
hi def link rtxAll			PreProc
hi def link rtxOp                       Operator
hi def link rtxOutput                   Number
hi def link rtxMacroCall                Macro

let b:current_syntax = "rtx"
" vim: ts=8
