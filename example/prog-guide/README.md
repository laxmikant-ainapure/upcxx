This is code from the programmer's guide. It can be modified here, but any updated code files should
be copied to the code/ subdirectory in the upcxx-prog-guide repo, and then put into guide.md with
the put-code.py script provided in the upcxx-prog-guide repo. And any code that is modified or added
within guide.md should be extracted with the get-code.py script, and copied here, and then tested to
see if it works.  

This process will be more integrated when the programmers guide is merged into this repo.  

Note that there are several cases where we have unimplemented features. These have been added with
placeholders that abort the execution.

To build all code, first run:

```
source <installdir>/bin/upcxx_meta
export PPFLAGS LDFLAGS LIBFLAGS
```

then

`make all`

Otherwise, e.g.

`make hello-world`

Run as usual, e.g. 

`GASNET_PSHM_NODES=4 ./hello-world`
