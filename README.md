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
src/rtx-comp rule-file pattern-file bytecode-file

# run the the rules
src/rtx-proc bytecode-file pattern-file < input

# decompile the rules and examine the bytecode
src/rtx-decomp bytecode-file text-file
```

Options for ```rtx-proc```:
 - ```-r``` print which rules are applying
 - ```-s``` trace the execution of the bytecode interpreter
 - ```-m``` trace the pattern matcher (Note: this currently has no effect)
 - ```-n``` indicates that the input stream has only source and target sides and not anaphora coreferences

Testing
-------

```bash
./run_tests.sh
```

Documentation
-------------

 - Project proposal: http://wiki.apertium.org/wiki/User:Popcorndude/Recursive_Transfer
 - File format documentation: http://wiki.apertium.org/wiki/User:Popcorndude/Recursive_Transfer/Formalism
 - Bytecode documentation: http://wiki.apertium.org/wiki/User:Popcorndude/Recursive_Transfer/Bytecode
 - Progress reports: http://wiki.apertium.org/wiki/User:Popcorndude/Recursive_Transfer/Progress and https://github.com/apertium/apertium-recursive/issues/1
