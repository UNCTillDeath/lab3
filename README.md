# lab3

EX1.
My own spin on ex1. Might not be correct :D Debugging is weird, will come back after working on the other exercises because I've spent way too much time on ex1.


EX2.
changes made to mutex-trie.c  based on ex2 req. Probably has bugs. It has very coarse locks. It locks from the start of a function until the end, the instructions seemed to make that okay for this exercise. Some functions don't try to lock because they are never invoked without being invoked by a different function. For instance insert locks then calls _insert. If _insert tried to lock, it would never get the lock because insert is waiting for _insert to return before releasing the lock(deadlock).

EX3.
drop_one_node() temporarily not included in file because we're not sure ours works and it's a lot of clutter
removed the mutex locks for the new leaf function because insert calls it and locks the lock

Insert()[(not recursive helper function)] just before it's done will check to see if node_count is > max_count. 
If it is, it will signal node_threshold_cv

check_max_nodes() waits on the signal, then loops until there are less than max_count nodes. 
will loop infinitely because drop_one_node() doesn't work/ isn't included in mutex-trie.c/ and the call to drop_one_node is commented out
