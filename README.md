# memorySimul

## Virtual Page system
---------------------------------------
*FIFO and LRU*
* one-level
* two-level
* inverted

**Run**

## argv: "[-s] simType firstLevelBits PhysicalMemorySizeBits TraceFileNames ...

-s option is detail print opt    

---------------------------------------------------------------------------------------
*simType*
> * 0 = one-level
> * 1 = two-level
> * 2 = inverted
> * over = All

**ex**
* memsim -s 3 10 20 0.trace base.trace 1.trace
