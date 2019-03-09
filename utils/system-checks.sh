#!/bin/bash

sys_info() {
    # Output information to assist in bug reports
    if test -z "$UPCXX_INSTALL_QUIET" ; then (
        if test -d .git ; then
            echo UPCXX revision: `git describe --always 2> /dev/null`
        fi
        echo System: `uname -a 2>&1`
        /usr/bin/sw_vers 2> /dev/null
        /usr/bin/xcodebuild -version 2> /dev/null 
        /usr/bin/lsb_release -a 2> /dev/null
        echo " "
        echo Date: `date 2>&1`
        echo Current directory: `pwd 2>&1`
        echo Install directory: $INSTALL_DIR
        SETTINGS=
        for var in CC CXX GASNET GASNET_CONFIGURE_ARGS CROSS OPTLEV DBGSYM \
	           UPCXX_BACKEND GASNET_INSTALL_TO \
		   UPCXX_CODEMODE UPCXX_THREADMODE \
		   UPCXX_CUDA \
		   ; do
            if test "${!var:+set}" = set; then
                SETTINGS="$SETTINGS $var='${!var}'"
            fi
        done
        echo "Settings:$SETTINGS"
        echo " "
	if test -n "$UPCXX_PYTHON" ; then
            fpy=`type -p $UPCXX_PYTHON`
            if test -x "$fpy" ; then
              echo "$fpy: " `$fpy --version 2>&1`
            else
              echo "UPCXX_PYTHON=$UPCXX_PYTHON not found!"
            fi
	fi
        echo " "
    ) fi
}

platform_sanity_checks() {
    if test -z "$UPCXX_INSTALL_NOCHECK" ; then (
        KERNEL=`uname -s 2> /dev/null`
        if test Linux = "$KERNEL" || test Darwin = "$KERNEL" ; then
            KERNEL_GOOD=1
        else
            KERNEL_GOOD=
        fi
        if test -n "$CRAY_PRGENVCRAY" ; then
            echo 'ERROR: UPC++ on Cray XC currently requires PrgEnv-gnu or PrgEnv-intel. Please do: `module switch PrgEnv-cray PrgEnv-gnu` or `module switch PrgEnv-cray PrgEnv-intel`'
            exit 1
        elif test -n "$CRAY_PRGENVPGI" ; then
            echo 'ERROR: UPC++ on Cray XC currently requires PrgEnv-gnu or PrgEnv-intel. Please do: `module switch PrgEnv-pgi PrgEnv-gnu` or `module switch PrgEnv-pgi PrgEnv-intel`'
            exit 1
        elif test -n "$CRAY_PRGENVINTEL" && ( test -z "$GCC_VERSION" || expr "$GCC_VERSION" : "^[2345]" > /dev/null || expr "$GCC_VERSION" : "^6.[123]" > /dev/null ) ; then
            echo 'ERROR: UPC++ on Cray XC with PrgEnv-intel must also have the gcc module loaded (version 6.4 or newer). Please do: `module load gcc`'
            exit 1
        elif test -n "$CRAY_PRGENVGNU" || test -n "$CRAY_PRGENVINTEL" ; then
            CC=${CC:-cc}
            CXX=${CXX:-CC}
	    # second condition eliminates build warnings in CI for: GASNET=build_or_inst_dir install -single
	    if [[ -z "$CROSS" && ( -z "$GASNET" || -f "$GASNET" || -f "$GASNET"/configure ) ]] ; then
	      echo 'WARNING: To build for Cray XC compute nodes, you should set the CROSS variable (e.g. CROSS=cray-aries-slurm)'
	    fi
        elif test "$KERNEL" = "Darwin" ; then # default to XCode clang
            CC=${CC:-/usr/bin/clang}
            CXX=${CXX:-/usr/bin/clang++}
        else
            CC=${CC:-gcc}
            CXX=${CXX:-g++}
        fi
        ARCH=`uname -m 2> /dev/null`
        ARCH_GOOD=
        ARCH_BAD=
        if test x86_64 = "$ARCH" ; then
            ARCH_GOOD=1
        elif test ppc64le = "$ARCH" ; then
            ARCH_GOOD=1
        elif expr "$ARCH" : 'i.86' >/dev/null 2>&1 ; then
            ARCH_BAD=1
        fi

        if test -z "$UPCXX_INSTALL_QUIET" ; then
            type -p ${CXX%% *}
            $CXX --version 2>&1 | grep -v 'warning #10315'
            type -p ${CC%% *}
            $CC --version 2>&1 | grep -v 'warning #10315'
            echo " "
        fi

        CXXVERS=`$CXX --version 2>&1`
        CCVERS=`$CC --version 2>&1`
        COMPILER_BAD=
        COMPILER_GOOD=
        if echo "$CXXVERS" | egrep 'Apple LLVM version [1-7]\.' 2>&1 > /dev/null ; then
            COMPILER_BAD=1
        elif echo "$CXXVERS" | egrep 'Apple LLVM version ([8-9]\.|[1-9][0-9])' 2>&1 > /dev/null ; then
            COMPILER_GOOD=1
        elif echo "$CXXVERS" | egrep 'PGI Compilers and Tools'  > /dev/null ; then
            COMPILER_BAD=1
        elif echo "$CXXVERS" | egrep 'IBM XL'  > /dev/null ; then
            COMPILER_BAD=1
        elif echo "$CXXVERS" | egrep 'Free Software Foundation' 2>&1 > /dev/null &&
	     echo "$CXXVERS" | head -1 | egrep ' +\([^\)]+\) +([1-5]\.|6\.[0-3])' 2>&1 > /dev/null ; then
            COMPILER_BAD=1
        elif echo "$CXXVERS" | egrep ' +\(ICC\) +(17\.0\.[2-9]|1[89]\.|2[0-9]\.)' 2>&1 > /dev/null ; then
	    # Ex: icpc (ICC) 18.0.1 20171018
            COMPILER_GOOD=1
        elif echo "$CXXVERS" | egrep ' +\(ICC\) ' 2>&1 > /dev/null ; then
	    :
        elif echo "$CXXVERS" | egrep 'Free Software Foundation' 2>&1 > /dev/null &&
             echo "$CXXVERS" | head -1 | egrep ' +\([^\)]+\) +([6-9]\.|[1-9][0-9])' 2>&1 > /dev/null ; then
            # Ex: g++ (Ubuntu 5.4.0-6ubuntu1~16.04.4) 5.4.0 20160609
            #     g++-7 (Homebrew GCC 7.2.0) 7.2.0
            #     foo (GCC) 7.2.0
            COMPILER_GOOD=1
        elif echo "$CXXVERS" | egrep 'clang version (2|3\.[1-7])' 2>&1 > /dev/null ; then
            COMPILER_BAD=1
        elif echo "$CXXVERS" | egrep 'clang version (3\.[8-9]|[4-9]\.|[1-9][0-9])' 2>&1 > /dev/null ; then
            COMPILER_GOOD=1
            # PrgEnv-llvm should be Untested (neither GOOD nor BAD)
            if test -n "$CRAY_PRGENVGNU" ; then
              COMPILER_GOOD=
            fi
        fi

        RECOMMEND='We recommend one of the following C++ compilers (or any later versions):
           Linux on x86_64:   g++ 6.4.0, LLVM/clang 3.8.0, Intel C 17.0.2
           Linux on ppc64le:  g++ 6.4.0
           macOS on x86_64:   g++ 6.4.0, Xcode/clang 8.0.0
           Cray XC systems:   PrgEnv-gnu with gcc/6.4.0 environment module loaded
                              PrgEnv-intel with Intel C 17.0.2 and gcc/6.4.0 environment module loaded'
        if test -n "$ARCH_BAD" ; then
            echo "ERROR: This version of UPC++ does not support the '$ARCH' architecture."
            echo "ERROR: $RECOMMEND"
            exit 1
        elif test -n "$COMPILER_BAD" ; then
            echo 'ERROR: Your C++ compiler is known to lack the support needed to build UPC++. '\
                 'Please set $CC and $CXX to point to a newer C/C++ compiler suite.'
            echo "ERROR: $RECOMMEND"
            exit 1
        elif test -z "$COMPILER_GOOD" || test -z "$KERNEL_GOOD" || test -z "$ARCH_GOOD" ; then
            echo 'WARNING: Your C++ compiler or platform has not been validated to run UPC++'
            echo "WARNING: $RECOMMEND"
        fi

        exit 0
    ) || exit 1 ; fi
}

platform_settings() {
   KERNEL=`uname -s 2> /dev/null`
   case "$KERNEL" in
     *)
       ;;
   esac
}

