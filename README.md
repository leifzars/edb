# edb
Embedded Key Value Data Base

Designed for embedded applications that require reliable, indexable storage.
Time Series Storage library is built on top of the EDB, it is optional.

# Features
  * B+Tree Key Value Index
  * Multiple Key Spaces (tables)
  * Secondary indexes
*Values are editable
*Transaction Support via WAL
*File System Optional
*Time Series Library
*Concurrency Support (Multiple Readers + 1 Writer)

#Notes
*Requires malloc 
*Not SQL

# Details
## Compile Notes
C code base.
Developed for ARM-M class chips.
Lately only tested on WIN32, currently without ARM board.

main.c only exists for testing.
Review os.h & env.h for your environment.

Suggested but not required; 
  Replace crc calls ('lib/crc.c')
  Replace assert handler ('lib/assert.c')
  

## Indexes
Each Key space (aka 'table' in the code) is paired with on index that maintains the keys.
Indexes are maintained via B+Tree of key value pairs both of type uint32. The value portion of course refers page index of the actual value.

Additional indexes can be added to a key space to help quickly find a value by another dimension.

## Write Ahead log (WAL)
based on [https://www.sqlite.org/wal.html]

## Concurrency
Due to the lovely benefits of the WAL, readers can access individual snapshots of the data while a writer continues mutating the store. This can be useful if multiple operations require a constant data source, and pausing while updating is not ideal.

Multiple writers are supported, but only if nested.

## Raw Mode
! Needs testing
Most embedded systems do not have fault tolerant file systems. FAT is most common and is not safe.
Raw Mode writes directly to storage, bypassing the filesystem and its allocator. To do this a region of storage must be assigned, and its size is fixed.

## Time Series Store
Optional store, see tsd.h for details.
