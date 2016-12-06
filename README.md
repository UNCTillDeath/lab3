# Lab 3: Threaded Programming

## EX1.

Our policy is essentially the furthest right leaf. When `drop_one_node()` is called, the function starts the root, and iterates down the next list until it finds a leaf, or reaches the end of this list. If it reaches the end of the list, it simply goes down to the child and does the same thing with that next list. This policy would be useful if the next list was sorted based off of usage (i.e. put more commonly used nodes at the front) because it would be removing nodes far down the next list.

## EX2.

It has very coarse locks. It locks from the start of a function until the end, where it unlocks immediately before returning. Some functions don't try to lock because they are never invoked without being invoked by a different function. For instance `insert()` locks then calls `_insert()`. If `_insert()` tried to lock, it would never get the lock because `insert()` is waiting for `_insert()` to return before releasing the lock(deadlock).

## EX3.

`drop_one_node()` is now being called by it's own thread called the `delete_thread`. This thread is implemented in `main.c` and is handled differently than the sequential implementation. Instead of it calling `delete()`, it directly calls the recursive `_delete()` since the non-recursive one needs a lock. Since we will already have the lock by the time `delete()` is called, the function will hang due to deadlock. Therefore, we use the recursive function.

The lock is acquired via `check_max_nodes` which will wait until a signal is given by `insert()` that indicates the `node_count` is larger than 100\. This signal is given in the line `pthread_cond_signal(&node_threshold_cv)` which triggers the delete thread to wake up and continue executing. The thread is told to wait from the line `pthread_cond_wait(&node_threshold_cv, &trie_lock)`.

## EX4.

`rw-trie.c` has similar code to `mutex-trie.c` but the mutex lock has been replaced by rwlocks. Mutex lock/unlock operations have been replaced by rwunlocks. Depending on the funciton, they will either get a read lock or a write lock. If the function is read-only, it will get a read lock via: `pthread_rwlock_rdlock(&read_write_lock)` or it will get a write lock via `pthread_rwlock_wrlock(&read_write_lock)` if the function is able of writing. Functions that are read-only include functions like `print()` or `search()` since they do not inherently change the structure of the trie. However, functions like `insert()` and `delete()` are write only as they cause changes in the structure of the trie.

The write lock is only given to a thread if no threads are reading or writing. Once a thread has the write lock, no other function can get a lock until the write lock is released. However, if there are no read locks, then any number of threads can obtain read locks (since reading does not change the structure of the trie, this is thread-safe).

## EX5
