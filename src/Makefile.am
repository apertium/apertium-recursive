AM_LDFLAGS=$(LIBS)
AM_CXXFLAGS=-Wall -g -ggdb -DDEBUG

bin_PROGRAMS = rtx-comp rtx-proc rec-inter rtx-comp2

rtx_comp_SOURCES = rtx_comp.cc rtx_compiler.cc rtx_parser.cc reduction_rule.cc parser_table.cc

rtx_proc_SOURCES = rtx_proc.cc stream_parser.cc parser_node.cc parser_table.cc
#rtx_processor.cc
#EXTRA_DIST = lsx_compiler.h

rec_inter_SOURCES = rec_inter.cc interchunk.cc

rtx_comp2_SOURCES = rtx_comp2.cc rtx_reader.cc
