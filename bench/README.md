# UPC++ Benchmarks

This directory contains our performance benchmarks. Each should be a `.cpp` file
containing its `main()` function. It is recommended that a benchmark include all
headers in `./common/`:

  * `./common/timer.hpp`: A timer class `bench::timer` used like a 
    stopwatch to capture time intervals.
  
  * `./common/report.hpp`: A class `bench::report` for appending measurements to
    a report file in a format easily consumed by the `util/show.py` script. The
    target report file path and external measurement parameters are passed
    through the following environment variables:
      
      - `report_file`: Path to file to create or append with report data.
        Defaults to `report.out`.
      
      - `report_args`: Python keyword argument assignments qualifying this
        running instance of the benchmark (e.g. `mood="happy", ranks=100` or
        `mood="sad", ranks=1`, take care to comma separate assignments and
        enclose strings in quotes). These should reflect properties of the
        execution environment beyond the control of the benchmark. For instance,
        if you're studying the performance of the benchmark under different
        compilers, you might run:
        
          `report_args="comp='gcc-O2'" ./my-benchmark-as-built-by-gcc-O2`
          `report_args="comp='gcc-O3'" ./my-benchmark-as-built-by-gcc-O3`
          `report_args="comp='clang-O3'" ./my-benchmark-as-built-by-clang-O3`
        
        Then the data points dumped to `report.out` will be qualified with a
        dimension `comp` taking 3 different string values.
        

  * `./common/operator_new.hpp`: Provides global replacements for
    `operator new/delete` parameterized by a compile time constant. All benchmarks
    are encouraged to include this so that experiments can be performed using
    different allocators unobtrusively w.r.t. the benchmark code. The supported
    allocators are:
      
      - `std`: The standard provided implementation (usually malloc/free).
      
      - `ltalloc`: [ltalloc](https://github.com/r-lyeh-archived/ltalloc) excels
        at lots of small and regular objects in a multi-threaded environment.
        I can't imagine us hand-rolling something better than this, except we
        could potentially take advantage of the following:
          
          1. Known object size at delete time. Thus hardcoding the query for
             "which bin to I put this free block in?".
          
          2. Inlining of both allocation and deallocation since C++ forbids
             that replacements of `operator new/delete` be marked `inline`.
             LTO should remedy this though.
      
      - `insane`: Hand-rolled. It only works if the program never asks for
        object sizes greater than 8K and never enters the allocator concurrently.
        If these conditions are met, it should be the fastest thing possible
        for our runtime's small objects, and thus is a useful means to generating
        speed-of-light bounds to determine if hand-rolling a more robust allocator
        would be worth the effort.
    
    To select between allocators when building with nobs (recommended) set the
    environment var `OPNEW=<std|ltalloc|insane>`. If `OPNEW=ltalloc`, then nobs
    will automatically download the allocator's source code into
    `./common/ltalloc.{h,cc}` (we do not ship with its code at this time).

## Report Analysis

See `./util/show.py -h` for info on how to plot reports.
