SHELL=/bin/bash
CXXFLAGS=`pkg-config --cflags apertium`
LDFLAGS=`pkg-config --libs apertium`
PREFIX1=eng-kaz
PREFIX2=kaz-eng
#all: $(PREFIX2) $(PREFIX1)
all: $(PREFIX1)

$(PREFIX1):
	#python3 ../AST/parse-grammar2.py $(PREFIX1).t1x < $(PREFIX1).grammar > parser.$(PREFIX1).y
	#python3 ../AST/create-lexer.py < $(PREFIX1).t1x | sed "s/parser.tab.h/parser.$(PREFIX1).tab.h/g" > lexicon.$(PREFIX1).l
	python3 ./parser.py -y < $(PREFIX1).grammar > parser.$(PREFIX1).y
	python3 ./parser.py -l < $(PREFIX1).grammar | sed "s/parser.tab.h/parser.$(PREFIX1).tab.h/g" > lexicon.$(PREFIX1).l
	flex lexicon.$(PREFIX1).l
	bison -d --report=all parser.$(PREFIX1).y
	g++ -I. $(CXXFLAGS) parser.$(PREFIX1).tab.c lex.yy.c $(LDFLAGS) -lfl -lapertium3 -o $(PREFIX1).parser

$(PREFIX2):
	python3 ../AST/parse-grammar.py $(PREFIX2).t1x < $(PREFIX2).grammar > parser.$(PREFIX2).y	
	python3 ../AST/create-lexer.py < $(PREFIX2).t1x | sed "s/parser.tab.h/parser.$(PREFIX2).tab.h/g" > lexicon.$(PREFIX2).l
	flex lexicon.$(PREFIX2).l
	bison -d --report=all parser.$(PREFIX2).y
	g++ -I. $(CXXFLAGS) parser.$(PREFIX2).tab.c lex.yy.c $(LDFLAGS) -lfl -lapertium3 -o $(PREFIX2).parser

clean:
	rm -f parser.{$(PREFIX1),${PREFIX2}}.{tab.c,tab.h,output} \
		parser.{$(PREFIX1),$(PREFIX2)}.y \
		lexicon.{$(PREFIX1),$(PREFIX2)}.l \
		{$(PREFIX1),$(PREFIX2)}.parser \
		lex.yy.c
