### Examples Directory ###

This directory exists to house examples of how users would write code
against UPC\+\+. This directory contains two subdirectories:
"example/app/" and "example/algo/".

## Directory: "example/app/" ##

The purpose of "example/app/" members are to demonstrate how the user 
builds, installs, and links against libupcxx, and also how to launch 
libupcxx-based applications. "example/app/" is given no special 
attention by the nobs buildsystem, so all the code in there should 
behave as if it were external application code not actually residing in 
this repository. Each app should contain its own buildsystem (makefile, 
cmake file, or naive shell script, etc.) and means of generating a 
local install of libupcxx or referencing the repository's nobs system. 
When an example needs to reference the upcxx repository, it should use 
a backtracking relative path from the example subdirectory up to 
"upcxx/". For example, a local install script in "example/app/foo/" 
would reference the upcxx directory as "../../../".

## Directory: "example/algo/" ##
"example/algo/" exists to demonstrate parallel programming with 
UPC\+\+. These should be free-floating collections of "foo.hpp" and 
"foo.cpp" pairs (subdirs permitted) which provide subroutines for 
exemplary cases of parallel programming (matrix solve's, etc.). These 
algorithmic examples will not have any build-system harnasses or 
"nobsrule.py" files. The intention is that these will proivde the 
"meat" for both "example/app/*" applications and our automated tests 
"test/*". It is recommended that each algorithm be referenced by at 
least one "test/*" so that we can confirm it is correct.

## Referencing members of "example/algo/" ##

Members of "example/app/" should reference files in "example/algo/" 
using relative paths ("example/app/foo" would reference 
"../../algo/bicgstab.{hpp,cpp}").

Tests in "test/" will reference the algo headers with
`#include <upcxx-example-algo/bicgstab.hpp>`. The usual nobs crawl of 
headers will find the accompanying "bigcstab.cpp" automatically. Paths 
based on "upcxx-example-algo" will *only* be valid for `#include's` 
from within "test/". (TODO: implement this).
