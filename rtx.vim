" Vim syntax file
" Language:	Apertium Recursive Transfer Ruleset
" Filenames:    *_rtx
" Maintainer:	Daniel Swanson <awesomeevilddudes@gmail.com>
" Last Change:	2019 Jul 22

" Quit when a (custom) syntax file was already loaded
if exists("b:current_syntax")
  finish
endif

syn region	rtxComment			start="!" end="\n" keepend contains=rtxComment
syn keyword	rtxIfKeyword		if else else-if else_if always
syn region	rtxString		start=+"+ end=+"+ skip=+\\"+
syn match	rtxChunkTag		"\v\$[^\. ()=]+"
syn match	rtxWeight		"\v\d+(\.\d+)?:"
syn match	rtxAll			"%"

" Define the default highlighting.
" Only used when an item doesn't have highlighting yet

hi def link rtxComment			Comment
hi def link rtxIfKeyword		Keyword
hi def link rtxChunkTag			Identifier
hi def link rtxString			String
hi def link rtxWeight			Constant
hi def link rtxAll			PreProc

let b:current_syntax = "rtx"
" vim: ts=8
