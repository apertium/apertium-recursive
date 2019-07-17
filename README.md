This is the repository for the Google Summer of Code project described here: http://wiki.apertium.org/wiki/User:Popcorndude/Recursive_Transfer

Compiling
---------

```bash
./autogen.sh
make
```

Running
-------

```bash
# compile the rules file
src/rtx-comp rule-file bytecode-file

# run the rules
src/rtx-proc bytecode-file < input

# decompile the rules and examine the bytecode
src/rtx-decomp bytecode-file text-file

# compile XML rule files
src/trx-comp bytecode-file xml-files...
```

Options for ```rtx-proc```:
 - ```-r``` print which rules are applying
 - ```-s``` trace the execution of the bytecode interpreter
 - ```-n``` indicates that the input stream has only source and target sides and not anaphora coreferences
 - ```-t``` mimic the behavior of apertium-transfer and apertium-interchunk
 - ```-T``` print the parse tree rather than applying output rules
 - ```-b``` print both the parse tree and the output
 - ```-m``` set the mode of tree output, available modes are:
  - ```nest``` (default) print the tree as text indented with tabs
  - ```flat``` print the tree as text
  - ```latex``` print the tree as tikz-qtree source

Testing
-------

```bash
./run_tests.sh
```

Using in a Pair
---------------

In ```Makefile.am``` add:
```
$(PREFIX1).rtx.bin: $(BASENAME).$(PREFIX1).rtx
	rtx-comp $< $@

$(PREFIX2).rtx.bin: $(BASENAME).$(PREFIX2).rtx
	rtx-comp $< $@
```

and add

```
$(PREFIX1).rtx.bin \
$(PREFIX2).rtx.bin
```

to ```TARGETS_COMMON```.

In ```modes.xml```, replace ```apertium-transfer```, ```apertium-interchunk```, and ```apertium-postchunk``` with:
```
<program name="rtx-proc">
  <file name="abc-xyz.rtx.bin"/>
</program>
```

Documentation
-------------

 - Project proposal: http://wiki.apertium.org/wiki/User:Popcorndude/Recursive_Transfer
 - File format documentation: http://wiki.apertium.org/wiki/User:Popcorndude/Recursive_Transfer/Formalism
 - Bytecode documentation: http://wiki.apertium.org/wiki/User:Popcorndude/Recursive_Transfer/Bytecode
 - Progress reports: http://wiki.apertium.org/wiki/User:Popcorndude/Recursive_Transfer/Progress and https://github.com/apertium/apertium-recursive/issues/1
 - Examples of functioning rule sets can be found in [apertium-eng-kir](https://github.com/apertium/apertium-eng-kir/blob/rtx/apertium-eng-kir.kir-eng.rtx), [`eng-spa.rtx`](eng-spa.rtx), and [`tests/`](tests/)
