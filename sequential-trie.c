/* -*- mode:c; c-file-style:"k&r"; c-basic-offset: 4; tab-width:4; indent-tabs-mode:nil; mode:auto-fill; fill-column:78; -*- */
/* vim: set ts=4 sw=4 et tw=78 fo=cqt wm=0: */
/* A simple, (reverse) trie.  Only for use with 1 thread. */

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "trie.h"

/*
*This LRU_List thing was an idea I tackled but failed at so far. Maybe we'll come back to this
*
*/
struct LRU_List {
	char key[64];
	unsigned int strlen;
	struct LRU_List *next;
};

struct trie_node {
    struct trie_node *next;  /* parent list */
    unsigned int strlen; /* Length of the key */
    int32_t ip4_address; /* 4 octets */
    struct trie_node *children; /* Sorted list of children */
    char key[64]; /* Up to 64 chars */
    struct LRU_List *next_recently_used; /* Last accessed node */
    struct LRU_List *tail; /* most recently used node */
};


static struct trie_node * root = NULL;
static int node_count = 0;
static int max_count = 100;  //Try to stay at no more than 100 nodes
static int MAX_KEY = 64;

struct trie_node * new_leaf (const char *string, size_t strlen, int32_t ip4_address) {
    struct trie_node *new_node = malloc(sizeof(struct trie_node));
    node_count++;
    if (!new_node) {
        printf ("WARNING: Node memory allocation failed.  Results may be bogus.\n");
        return NULL;
    }
    assert(strlen < MAX_KEY);
    assert(strlen > 0);
    new_node->next = NULL;
    new_node->strlen = strlen;
    strncpy(new_node->key, string, strlen);
    new_node->key[strlen] = '\0';
    new_node->ip4_address = ip4_address;
    new_node->children = NULL;

    return new_node;
}

// Compare strings backward.  Unlike strncmp, we assume
// that we will not stop at a null termination, only after
// n chars (or a difference).  Base code borrowed from musl
int reverse_strncmp(const char *left, const char *right, size_t n)
{
    const unsigned char *l= (const unsigned char *) &left[n-1];
    const unsigned char *r= (const unsigned char *) &right[n-1];
    if (!n--) return 0;
    for (; *l && *r && n && *l == *r ; l--, r--, n--);
    return *l - *r;
}

int compare_keys (const char *string1, int len1, const char *string2, int len2, int *pKeylen) {
    int keylen, offset;
    char scratch[MAX_KEY];
    assert (len1 > 0);
    assert (len2 > 0);
    // Take the max of the two keys, treating the front as if it were
    // filled with spaces, just to ensure a total order on keys.
    if (len1 < len2) {
        keylen = len2;
        offset = keylen - len1;
        memset(scratch, ' ', offset);
        memcpy(&scratch[offset], string1, len1);
        string1 = scratch;
    } else if (len2 < len1) {
        keylen = len1;
        offset = keylen - len2;
        memset(scratch, ' ', offset);
        memcpy(&scratch[offset], string2, len2);
        string2 = scratch;
    } else
        keylen = len1; // == len2

    assert (keylen > 0);
    if (pKeylen)
        *pKeylen = keylen;
    return reverse_strncmp(string1, string2, keylen);
}

int compare_keys_substring (const char *string1, int len1, const char *string2, int len2, int *pKeylen) {
    int keylen, offset1, offset2;
    keylen = len1 < len2 ? len1 : len2;
    offset1 = len1 - keylen;
    offset2 = len2 - keylen;
    assert (keylen > 0);
    if (pKeylen)
        *pKeylen = keylen;
    return reverse_strncmp(&string1[offset1], &string2[offset2], keylen);
}

void init(int numthreads) {
    if (numthreads != 1)
        printf("WARNING: This Trie is only safe to use with one thread!!!  You have %d!!!\n", numthreads);

    root = NULL;
}

void shutdown_delete_thread() {
    // Don't need to do anything in the sequential case.
    return;
}

/* Recursive helper function.
 * Returns a pointer to the node if found.
 * Stores an optional pointer to the
 * parent, or what should be the parent if not found.
 *
 */
struct trie_node *
_search (struct trie_node *node, const char *string, size_t strlen) {

    int keylen, cmp;

    // First things first, check if we are NULL
    if (node == NULL) return NULL;

    assert(node->strlen <= MAX_KEY);

    // See if this key is a substring of the string passed in
    cmp = compare_keys_substring(node->key, node->strlen, string, strlen, &keylen);
    if (cmp == 0) {
        // Yes, either quit, or recur on the children

        // If this key is longer than our search string, the key isn't here
        if (node->strlen > keylen) {
            return NULL;
        } else if (strlen > keylen) {
            // Recur on children list
            return _search(node->children, string, strlen - keylen);
        } else {
            assert (strlen == keylen);

            return node;
        }

    } else {
        cmp = compare_keys(node->key, node->strlen, string, strlen, &keylen);
        if (cmp < 0) {
            // No, look right (the node's key is "less" than the search key)
            return _search(node->next, string, strlen);
        } else {
            // Quit early
            return 0;
        }
    }
}


int search  (const char *string, size_t strlen, int32_t *ip4_address) {
    struct trie_node *found;

    assert(strlen <= MAX_KEY);

    // Skip strings of length 0
    if (strlen == 0)
        return 0;

    found = _search(root, string, strlen);

    if (found && ip4_address)
        *ip4_address = found->ip4_address;

    return (found != NULL);
}

/* Recursive helper function */
int _insert (const char *string, size_t strlen, int32_t ip4_address,
             struct trie_node *node, struct trie_node *parent, struct trie_node *left) {

    int cmp, keylen;

    // First things first, check if we are NULL
    assert (node != NULL);
    assert (node->strlen <= MAX_KEY);

    // Take the minimum of the two lengths
    cmp = compare_keys_substring (node->key, node->strlen, string, strlen, &keylen);
    if (cmp == 0) {
        // Yes, either quit, or recur on the children

        // If this key is longer than our search string, we need to insert
        // "above" this node
        if (node->strlen > keylen) {
            struct trie_node *new_node;

            assert(keylen == strlen);
            assert((!parent) || parent->children == node);

            new_node = new_leaf (string, strlen, ip4_address);
            node->strlen -= keylen;
            new_node->children = node;
            new_node->next = node->next;
            node->next = NULL;

            assert ((!parent) || (!left));

            if (parent) {
                parent->children = new_node;
            } else if (left) {
                left->next = new_node;
            } else if ((!parent) || (!left)) {
                root = new_node;
            }
            return 1;

        } else if (strlen > keylen) {

            if (node->children == NULL) {
                // Insert leaf here
                struct trie_node *new_node = new_leaf (string, strlen - keylen, ip4_address);
                node->children = new_node;
                return 1;
            } else {
                // Recur on children list, store "parent" (loosely defined)
                return _insert(string, strlen - keylen, ip4_address,
                               node->children, node, NULL);
            }
        } else {
            assert (strlen == keylen);
            if (node->ip4_address == 0) {
                node->ip4_address = ip4_address;
                return 1;
            } else {
                return 0;
            }
        }

    } else {
        /* Is there any common substring? */
        int i, cmp2, keylen2, overlap = 0;
        for (i = 1; i < keylen; i++) {
            cmp2 = compare_keys_substring (&node->key[i], node->strlen - i,
                                           &string[i], strlen - i, &keylen2);
            assert (keylen2 > 0);
            if (cmp2 == 0) {
                overlap = 1;
                break;
            }
        }

        if (overlap) {
            // Insert a common parent, recur
            int offset = strlen - keylen2;
            struct trie_node *new_node = new_leaf (&string[offset], keylen2, 0);
            assert ((node->strlen - keylen2) > 0);
            node->strlen -= keylen2;
            new_node->children = node;
            new_node->next = node->next;
            node->next = NULL;
            assert ((!parent) || (!left));

            if (node == root) {
                root = new_node;
            } else if (parent) {
                assert(parent->children == node);
                parent->children = new_node;
            } else if (left) {
                left->next = new_node;
            } else if ((!parent) && (!left)) {
                root = new_node;
            }

            return _insert(string, offset, ip4_address,
                           node, new_node, NULL);
        } else {
            cmp = compare_keys (node->key, node->strlen, string, strlen, &keylen);
            if (cmp < 0) {
                // No, recur right (the node's key is "less" than  the search key)
                if (node->next)
                    return _insert(string, strlen, ip4_address, node->next, NULL, node);
                else {
                    // Insert here
                    struct trie_node *new_node = new_leaf (string, strlen, ip4_address);
                    node->next = new_node;
                    return 1;
                }
            } else {
                // Insert here
                struct trie_node *new_node = new_leaf (string, strlen, ip4_address);
                new_node->next = node;
                if (node == root)
                    root = new_node;
                else if (parent && parent->children == node)
                    parent->children = new_node;
                else if (left && left->next == node)
                    left->next = new_node;
            }
        }
        return 1;
    }
}

void assert_invariants();

int insert (const char *string, size_t strlen, int32_t ip4_address) {

    assert(strlen <= MAX_KEY);

    // Skip strings of length 0
    if (strlen == 0)
        return 0;

    /* Edge case: root is null */
    if (root == NULL) {
        root = new_leaf (string, strlen, ip4_address);
        return 1;
    }

    int rv = _insert (string, strlen, ip4_address, root, NULL, NULL);
    assert_invariants();
    printf("after insertion count: %d\n", node_count);
    return rv;
}

/* Recursive helper function.
 * Returns a pointer to the node if found.
 * Stores an optional pointer to the
 * parent, or what should be the parent if not found.
 *
 */
struct trie_node *
_delete (struct trie_node *node, const char *string,
         size_t strlen) {
    int keylen, cmp;

    // First things first, check if we are NULL
    if (node == NULL) return NULL;

    assert(node->strlen < MAX_KEY);

    // See if this key is a substring of the string passed in
    cmp = compare_keys_substring (node->key, node->strlen, string, strlen, &keylen);
    if (cmp == 0) {
        // Yes, either quit, or recur on the children

        // If this key is longer than our search string, the key isn't here
        if (node->strlen > keylen) {
            return NULL;
        } else if (strlen > keylen) {
            struct trie_node *found =  _delete(node->children, string, strlen - keylen);
            if (found) {
                /* If the node doesn't have children, delete it.
                 * Otherwise, keep it around to find the kids */
                if (found->children == NULL && found->ip4_address == 0) {
                    assert(node->children == found);
                    node->children = found->next;
                    free(found);
                    node_count--;
                }

                /* Delete the root node if we empty the tree */
                if (node == root && node->children == NULL && node->ip4_address == 0) {
                    root = node->next;
                    free(node);
                    node_count--;
                }

                return node; /* Recursively delete needless interior nodes */
            } else
                return NULL;
        } else {
            assert (strlen == keylen);

            /* We found it! Clear the ip4 address and return. */
            if (node->ip4_address) {
                node->ip4_address = 0;

                /* Delete the root node if we empty the tree */
                if (node == root && node->children == NULL && node->ip4_address == 0) {
                    root = node->next;
                    free(node);
                    node_count--;
                    return (struct trie_node *) 0x100100; /* XXX: Don't use this pointer for anything except
                                                           * comparison with NULL, since the memory is freed.
                                                           * Return a "poison" pointer that will probably
                                                           * segfault if used.
                                                           */
                }
                return node;
            } else {
                /* Just an interior node with no value */
                return NULL;
            }
        }

    } else {
        cmp = compare_keys (node->key, node->strlen, string, strlen, &keylen);
        if (cmp < 0) {
            // No, look right (the node's key is "less" than  the search key)
            struct trie_node *found = _delete(node->next, string, strlen);
            if (found) {
                /* If the node doesn't have children, delete it.
                 * Otherwise, keep it around to find the kids */
                if (found->children == NULL && found->ip4_address == 0) {
                    assert(node->next == found);
                    node->next = found->next;
                    free(found);
                    node_count--;
                }
                return node; /* Recursively delete needless interior nodes */
            }
            return NULL;
        } else {
            // Quit early
            return NULL;
        }
    }
}

int delete  (const char *string, size_t strlen) {
    // Skip strings of length 0
    if (strlen == 0)
        return 0;

      if(search(string, strlen, 0)) printf("node found via search \n\n\n\n\n\n\n\n\n\n");
      else printf("Search yeilded no results \n");
    assert(strlen <= MAX_KEY);
    printf("current count before deletion: %d, deleting: %s\n", node_count, string);
    int rv = (NULL != _delete(root, string, strlen));
    assert_invariants();
    printf("current count after deletion: %d\n", node_count);
    if(!rv)
    {
    	printf("Failure to delete\n");
    }
    return rv;
}

char* combineKey(char* prefix, char* suffix){
  if(suffix == NULL) printf("NULL VALUE FOR SUFFIX");
  printf("Length of Suffix: %zd", strlen(suffix));
  if(strlen(suffix) == 0){
    return prefix;
  }
  strncat(prefix, suffix, strlen(prefix)  + strlen(suffix)); // append old stuff
  return prefix;
}


/* Find one node to remove from the tree.
 * Use any policy you like to select the node.
 * I am looping until the last child, then, if the last child has a next, I loop until the last next.
 */
int drop_one_node  () {


	printf("we in here\n\n\n\n\n\n\n\n\n\n\n\n");
	char key_to_delete[MAX_KEY+2]; // plus 1 because of behaviourss of strncpy and strndup adding a \0 at n + q if src > dest
  key_to_delete[0] = '\0';
  key_to_delete[MAX_KEY+1] = '\0';

    struct trie_node *current = root;

    if(!(current->children)) // current doesn't have children
    {
      printf("Current Key: %s\n", current->key);
      printf("Found key on first level\n");
      strncpy(key_to_delete, current->key, MAX_KEY);
    }else{
      while(current != NULL){
          if(!(current->children)){
              printf("No Children, Deleting\n");
              strncpy(key_to_delete, combineKey(current->key, key_to_delete), MAX_KEY);
              printf("Key to delete %s\n", key_to_delete);
              break;
          }else if(current->next == NULL){
              printf("Prefix: %s \n Suffix: %s\n", current->key, key_to_delete);
              printf("Combining keys\n");
              strncpy(key_to_delete, combineKey(current->key, key_to_delete), MAX_KEY);
              printf("Key to delete %s\n", key_to_delete);
              current = current->children;
          }else{
              printf("Going to next node with key: %s\n", current->next->key);
              current = current->next;
          }
        }
      }


      printf("Key: %s\n", key_to_delete);
      if(delete(key_to_delete, strlen(key_to_delete))){
        printf("Delete Successful\n");

        return 0;
    } else return 1;
/*
    if (!root || !(root->children))
    {
    	return 0;
    }

    struct trie_node *current = root->children;
    assert(root->children);

    if(!(current->children)) // current doesn't have children
    {
    	strncpy(key_to_delete, current->key, MAX_KEY);
    }
    else
    {
	    for( ; current->children; current = current->children)
	    {
        if(current->children == NULL){
          break;
        }
	    	char *temp_prefix = strndup(current->key, MAX_KEY);
	    	char *temp_suffix = strndup(key_to_delete, MAX_KEY); // old key_to_delete

	    	strncpy(key_to_delete, temp_prefix, (MAX_KEY - strlen(key_to_delete))); // make prefix take over key_to_delete // strlen will write over till MAX_KEY - len(key) null bytes
	    	strncat(key_to_delete, temp_suffix, MAX_KEY - strlen(key_to_delete)); // append old stuff

	    	free(temp_prefix);
	    	free(temp_suffix);
        printf("Key to delete %s\n", key_to_delete);
	    }
	}


	for (; current->next; current = current->next)  // loop until current is the tail
	{

	}

	char *temp_prefix = strndup(current->key, MAX_KEY);
	char *temp_suffix = strndup(key_to_delete, MAX_KEY); // old key_to_delete

	strncpy(key_to_delete, temp_prefix, (MAX_KEY - strlen(key_to_delete))); // make prefix take over key_to_delete // strlen will write over till MAX_KEY - len(key) null bytes
	strncat(key_to_delete, temp_suffix, MAX_KEY - strlen(key_to_delete)); // append old stuff

	free(temp_prefix);
	free(temp_suffix);


  printf("Key to delete %s\n", key_to_delete);


	printf("about to delete: %s\n", key_to_delete);
    if(!delete(key_to_delete, strlen(key_to_delete))) {
      printf("DELETE DIDNT WORK FAM \n");
      return 1;
    }
*/

}

/* Check the total node count; see if we have exceeded a the max.
 */
void check_max_nodes  () {
    while (node_count > max_count) {
    	printf("Current count is: %d\n", node_count);
    	sleep(3);
        //printf("Warning: not dropping nodes yet.  Drop one node not implemented\n");
        //break;
        if(drop_one_node()){
          printf("drop_one_node failed");
          break;
        }

    }
}

void _print (struct trie_node *node) {
    printf ("Node at %p.  Key %.*s (%d), IP %d.  Next %p, Children %p\n",
            node, node->strlen, node->key, node->strlen, node->ip4_address, node->next, node->children);
    if (node->children)
        _print(node->children);
    if (node->next)
        _print(node->next);
}

void print() {
    printf ("Root is at %p\n", root);
    /* Do a simple depth-first search */
    if (root)
        _print(root);
}


int _assert_invariants (struct trie_node *node, int prefix_length, int *error) {
    int count = 1;

    int len = prefix_length + node->strlen;
    if (len > MAX_KEY) {
        printf("key too long at node %p.  Key %.*s (%d), IP %d.  Next %p, Children %p\n",
               node, node->strlen, node->key, node->strlen, node->ip4_address, node->next, node->children);
        *error = 1;
        return count;
    }

    if (node->children) {
        count += _assert_invariants(node->children, len, error);
        if (*error) {
            printf("Unwinding tree on error: node %p.  Key %.*s (%d), IP %d.  Next %p, Children %p\n",
                   node, node->strlen, node->key, node->strlen, node->ip4_address, node->next, node->children);
            return count;
        }
    }

    if (node->next) {
        count += _assert_invariants(node->next, prefix_length, error);
    }

    return count;
}

void assert_invariants () {
#ifdef DEBUG
    int err = 0;
    if (root) {
        int count = _assert_invariants(root, 0, &err);
        if (err) print();
        assert(count == node_count);
    }
#endif // DEBUG
}
