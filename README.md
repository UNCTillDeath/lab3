# lab3

EX1.
My own spin on ex1. Might not be correct :D Debugging is weird, will come back after working on the other exercises because I've spent way too much time on ex1.


EX2.
changes made to mutex-trie.c  based on ex2 req. Probably has bugs. It has very coarse locks. It locks from the start of a function until the end, the instructions seemed to make that okay for this exercise. Some functions don't try to lock because they are never invoked without being invoked by a different function. For instance insert locks then calls _insert. If _insert tried to lock, it would never get the lock because insert is waiting for _insert to return before releasing the lock(deadlock).
