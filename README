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
src/rtx-comp2 rule-file pattern-file bytecode-file

# run the the rules
src/rec-inter bytecode-file pattern-file < input

# decompile the rules and examine the bytecode
src/decomp bytecode-file text-file
```

Options for ```rec-inter```:
 - ```-r``` print which rules are applying
 - ```-s``` trace the execution of the bytecode interpreter
 - ```-m``` trace the pattern matcher

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
