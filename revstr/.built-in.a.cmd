cmd_assignment/revstr/built-in.a := rm -f assignment/revstr/built-in.a; echo revstr.o | sed -E 's:([^ ]+):assignment/revstr/\1:g' | xargs ar cDPrST assignment/revstr/built-in.a
