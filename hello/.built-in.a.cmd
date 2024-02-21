cmd_assignment/hello/built-in.a := rm -f assignment/hello/built-in.a; echo hello.o | sed -E 's:([^ ]+):assignment/hello/\1:g' | xargs ar cDPrST assignment/hello/built-in.a
