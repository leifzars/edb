Rename
Allow for coalescing of pages for read
Optimize WAL Read pattern, then optimize write
Optimize WAL append while DB Read
Allow for page buffer to be backed by SLAB, allow slab to grow?
WAL index should not be a vector, i think a Tree struct might be best, KTree, not KHash