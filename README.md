LSI-AS
======

This is a very simple assembler which produces code in absolute loader format
for the PDP-11/03 aka LSI-11.


Usage
-----

```
lsi-as [-q] [-p] in.s out.bin
```

- `-p` prints all labels/constants after assembly is finished
- `-q` makes the assembler "quiet" and suppresses output of the size and entry
  point
- `in.s` is the assembler source code
- `out.bin` is the compiled binary in absolute loader format


Syntax
------

LSI-AS accepts all mnemonics of the PDP-11/03 as described in the processor
handbook. In addition, it supports comments, labels, constants, words, and text
strings.

The entry point is defined by the `_START` label, the initial address is
defined by the `.ORG` instruction. The following example demonstrates how to
set the entry point, how to use labels, and how to define a text string.

```
        .ORG    4000

_START: MOV     #1000,  SP
        MOV     #TEXT,  R0
        JSR     PC,     PRINT
        HALT

PRINT:  TSTB    (R0)
        BEQ     PREND
PRWAIT: TSTB    @#177564
        BPL     PRWAIT
        MOVB    (R0)+,  @#177566
        BR      PRINT
PREND:  RTS     PC

TEXT:   "Hello world!\r\n"
```


Why?
----

Of course one could just use GNU as from binutils, but this lsi-as is much
simpler and smaller than GNU as and unlike GNU as, it also directly outputs
absolute loader format, which is exactly what you need when dealing with bare
metal PDP-11 machines. With some improvements, lsi-as might even be portable to
PDP-11.
