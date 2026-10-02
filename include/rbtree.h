#pragma once

#include <types.h>
#include <string.h>

/* Red-black tree node colors */
enum rb_color {
	RB_BLACK,
	RB_RED,
};

/**
 * Red-black tree node
 *
 * This structure should be embedded in the data structure you want to store
 * in the tree. Use container_of() to get the containing structure from a node.
 *
 * The size field maintains the number of nodes in the subtree rooted at this
 * node (including the node itself). This augmentation enables O(log n)
 * order-statistic queries.
 */
struct rb_node {
	struct rb_node* left;     /* Left child (NULL if none) */
	struct rb_node* right;    /* Right child (NULL if none) */
	struct rb_node* parent;   /* Parent node (NULL if root) */

	enum rb_color color;      /* Node color (RB_RED or RB_BLACK) */
	int size;                 /* Size of subtree rooted at this node */
};

/**
 * Red-black tree root structure
 *
 * This structure represents the tree itself and should be initialized with
 * rb_init() before use.
 */
struct rb_tree {
	struct rb_node *root;     /* Root node of the tree (NULL if empty) */
	int size;                 /* Total number of nodes in the tree */
};

/**
 * RB_DEFINE_SEARCH_FUNC - Define a type-safe search function for a red-black tree
 *
 * This macro generates a static function that searches for an element in the tree
 * using binary search. The search runs in O(log n) time.
 *
 * Parameters:
 *   @type: The type of the structure containing the rb_node
 *   @rb_search_func_name: The name of the function to generate
 *   @cmp_func: Comparison function with signature: int cmp_func(type *a, type *b)
 *              Should return: <0 if a < b, 0 if a == b, >0 if a > b
 *   @rb_node_member: The name of the rb_node field in the structure
 *
 * Example usage:
 *   struct my_data {
 *       int key;
 *       struct rb_node node;
 *   };
 *
 *   int my_cmp(struct my_data *a, struct my_data *b) {
 *       return a->key - b->key;
 *   }
 *
 *   RB_DEFINE_SEARCH_FUNC(struct my_data, my_search, my_cmp, node)
 *
 * Generated function:
 *   static struct my_data* my_search(struct rb_tree *root, struct my_data *key);
 *
 * Returns: Pointer to the found element, or NULL if not found
 */
#define RB_DEFINE_SEARCH_FUNC(type, rb_search_func_name, cmp_func, rb_node_member)                      \
	/**                                                                                                 
	 * Search for an element in the red-black tree                                                      
	 * @param root The tree to search in                                                                
	 * @param key The element to search for (only the key field needs to be set)                        
	 * @return Pointer to the found element, or NULL if not found                                       
	 */                                                                                                 \
	static type* rb_search_func_name(struct rb_tree *root, type* key)                                   \
	{                                                                                                   \
		struct rb_node *current = root->root;                                                           \
																										\
		while (current != NULL) {                                                                       \
			type *current_entry = container_of(current, type, rb_node_member);                          \
			int cmp = cmp_func(key, current_entry);                                                     \
																										\
			if (cmp == 0) {                                                                             \
				return current_entry; /* Found the key */                                               \
			} else if (cmp < 0) {                                                                       \
				current = current->left; /* Go left */                                                  \
			} else {                                                                                    \
				current = current->right; /* Go right */                                                \
			}                                                                                           \
		}                                                                                               \
																										\
		return NULL; /* Key not found */                                                                \
	}                                                                                                   \

/**
 * RB_DEFINE_GUARDED_INSERT_FUNC - Define a guarded insertion function with validation
 *
 * This macro generates a static function that inserts an element into the red-black
 * tree with optional validation. The insertion runs in O(log n) time and maintains
 * all red-black tree properties and size augmentation.
 *
 * The function walks down the tree to find the insertion point, optimistically
 * updating the size field of each node along the path. If validation fails at any
 * point, the sizes are rolled back and the insertion is aborted.
 *
 * Parameters:
 *   @type: The type of the structure containing the rb_node
 *   @rb_guarded_insert_func_name: The name of the function to generate
 *   @cmp_func: Comparison function with signature: int cmp_func(type *a, type *b)
 *              Should return: <0 if a < b, 0 if a == b, >0 if a > b
 *   @rb_node_member: The name of the rb_node field in the structure
 *   @validate_func: Optional validation function with signature:
 *                   int validate_func(type *new_entry, type *existing_entry)
 *                   Should return: 0 if validation passes, non-zero if it fails
 *                   Pass NULL to disable validation (equivalent to RB_DEFINE_INSERT_FUNC)
 *
 * Example usage:
 *   struct vma {
 *       uintptr_t start, end;
 *       struct rb_node node;
 *   };
 *
 *   int vma_validate(struct vma *new, struct vma *existing) {
 *       // Check for overlapping ranges
 *       if (new->start < existing->end && new->end > existing->start)
 *           return -1;  // Overlap detected
 *       return 0;
 *   }
 *
 *   RB_DEFINE_GUARDED_INSERT_FUNC(struct vma, vma_insert, vma_cmp, node, vma_validate)
 *
 * Generated function:
 *   static int vma_insert(struct rb_tree *root, struct vma *new_entry,
 *                         struct vma **out_successor);
 *
 * Generated function parameters:
 *   @root: The tree to insert into
 *   @new_entry: The element to insert (must have rb_node initialized via rb_node_init)
 *   @out_successor: Optional output parameter. If non-NULL, will be set to point to
 *                   the in-order successor of the newly inserted node (the smallest
 *                   element greater than new_entry). This is useful for maintaining
 *                   auxiliary data structures like linked lists. Set to NULL if the
 *                   new element has no successor (i.e., it's the largest element).
 *
 * Returns: 0 on success, -1 if validation failed
 *
 * Note: The rb_node field of new_entry must be initialized with rb_node_init() before
 *       calling this function. After successful insertion, the node's parent, left, right,
 *       color, and size fields will be set by the tree.
 */
#define RB_DEFINE_GUARDED_INSERT_FUNC(type, rb_guarded_insert_func_name, cmp_func, rb_node_member, validate_func)  \
	/**                                                                                                            
	 * Insert an element into the red-black tree with optional validation                                          
	 * @param root The tree to insert into                                                                         
	 * @param new_entry The element to insert (rb_node must be initialized via rb_node_init)                       
	 * @param out_successor Optional output: set to the in-order successor of the new node, or NULL if none        
	 * @return 0 on success, -1 if validation failed                                                               
	 */                                                                                                            \
	static int rb_guarded_insert_func_name(struct rb_tree *root, type *new_entry, type **out_successor)            \
	{                                                                                                              \
		typedef int (*validate_func_t)(type *, type *);                                                            \
		/* Create a new node */                                                                                    \
		struct rb_node *y = NULL; /* trailing parent pointer */                                                    \
		struct rb_node *x = root->root; /* current node */                                                         \
		struct rb_node *new_node = &new_entry->rb_node_member;                                                     \
		type *successor = NULL;                                                                                    \
																											       \
		while (x != NULL) {                                                                                        \
			y = x;                                                                                                 \
			type *x_entry = container_of(x, type, rb_node_member);                                                 \
			validate_func_t validate = (validate_func_t)validate_func;                                             \
			if (validate != NULL && validate(new_entry, x_entry) != 0) {                                           \
                /* Validation failed! Rollback sizes before returning. */                                          \
                /* We went down the path incrementing sizes, now we must undo that. */                             \
				struct rb_node *rollback_node = y;                                                                 \
             	if (rollback_node == NULL) return -1;                                                              \
             	/* Use the parent pointers to walk back up */                                                      \
				while (rollback_node != NULL) {                                                                    \
					rollback_node->size--;                                                                         \
					rollback_node = rollback_node->parent;                                                         \
				}                                                                                                  \
				return -1;                                                                                         \
            }                                                                                                      \
																											       \
			/* 
			 * As we go finding the path to the new node,
			 * we increment the size of all the nodes we pass by 1
			 * (since the node we're inserting will be on that path)
			 * We're also incrementing optimistically here, we're assuming
			 * the place where we insert is always valid. We will do rollback
			 * in the case where a violation occurs 
			 */                                                                                                    \
			y->size++; /* (CLRS page 484) */                                                                       \
																											       \
																											       \
			/* Find correct spot to insert it */                                                                   \
			int cmp = cmp_func(new_entry, x_entry);                                                                \
			if (cmp < 0) {                                                                                         \
				/* Going left means x_entry > new_entry, so x_entry is a potential successor */                    \
				successor = x_entry;                                                                               \
				x = x->left;                                                                                       \
			} else {                                                                                               \
				x = x->right;                                                                                      \
			}                                                                                                      \
		}                                                                                                          \
																											       \
		/* Set the parent of the new node */                                                                       \
		new_node->parent = y;                                                                                      \
																											       \
		/* If y is null, the tree is empty, so set the root to the new node */                                     \
		if (y == NULL) {                                                                                           \
			root->root = new_node;                                                                                 \
		} else {                                                                                                   \
			type *y_entry = container_of(y, type, rb_node_member);                                                 \
			if (cmp_func(new_entry, y_entry) < 0) {                                                                \
				/* If the new node's value is less than the parent's value, */                                     \
				/* set the new node as the left child */                                                           \
				y->left = new_node;                                                                                \
			} else {                                                                                               \
				/* If the new node's value is greater than the parent's value, */                                  \
				/* set the new node as the right child */                                                          \
				y->right = new_node;                                                                               \
			}                                                                                                      \
		}                                                                                                          \
																											       \
		new_node->left = NULL;                                                                                     \
		new_node->right = NULL;                                                                                    \
		new_node->color = RB_RED;                                                                                  \
		new_node->size = 1; /* Empty nodes have a size of 1 */                                                     \
																											       \
		rb_fix_tree_insert(root, new_node);                                                                        \
		root->size++;                                                                                              \
		if (out_successor) *out_successor = successor;                                                             \
	    return 0;                                                                                                  \
	}                                                                                                              \


/**
 * RB_DEFINE_INSERT_FUNC - Define a simple insertion function without validation
 *
 * This is a convenience macro that generates an insertion function without any
 * validation checks. It's equivalent to calling RB_DEFINE_GUARDED_INSERT_FUNC
 * with validate_func set to NULL.
 *
 * Parameters:
 *   @type: The type of the structure containing the rb_node
 *   @rb_insert_func_name: The name of the function to generate
 *   @cmp_func: Comparison function (see RB_DEFINE_SEARCH_FUNC for details)
 *   @rb_node_member: The name of the rb_node field in the structure
 *
 * See RB_DEFINE_GUARDED_INSERT_FUNC for detailed documentation of parameters
 * and usage examples.
 */
#define RB_DEFINE_INSERT_FUNC(type, rb_insert_func_name, cmp_func, rb_node_member)              \
	RB_DEFINE_GUARDED_INSERT_FUNC(type, rb_insert_func_name, cmp_func, rb_node_member, NULL)

/**
 * rb_init - Initialize a red-black tree
 *
 * This function initializes an rb_tree structure to an empty state. Must be
 * called before any other operations on the tree.
 *
 * @tree: The tree to initialize
 */
static inline void rb_init(struct rb_tree *tree)
{
	tree->root = NULL;
	tree->size = 0;
}

/**
 * rb_node_init - Initialize a red-black tree node
 *
 * This function initializes an rb_node structure to a clean state. Must be
 * called on a node before inserting it into a tree.
 *
 * @node: The node to initialize
 */
static inline void rb_node_init(struct rb_node *node)
{
	memset(node, 0, sizeof *node);
}

/**
 * rb_fix_tree_insert - Restore red-black properties after insertion
 *
 * This function is called internally after a new node is inserted to restore
 * the red-black tree properties through recoloring and rotations. Based on
 * the RB-INSERT-FIXUP algorithm from CLRS page 316.
 *
 * @root: The tree containing the new node
 * @newNode: The newly inserted node (must be colored RED initially)
 */
void rb_fix_tree_insert(struct rb_tree* root, struct rb_node* newNode);

/**
 * rb_remove - Remove a node from the red-black tree
 *
 * This function removes a node from the tree and restores red-black properties.
 * The size fields of affected nodes are updated during removal. Based on the
 * RB-DELETE algorithm from CLRS page 324 with size augmentation from page 485.
 *
 * @root: The tree containing the node
 * @nodeToRemove: The node to remove (must be in the tree, or NULL)
 *
 * Note: After removal, the node's memory is not freed - the caller is responsible
 *       for managing the memory of the containing structure.
 */
void rb_remove(struct rb_tree* root, struct rb_node* nodeToRemove);

struct rb_node *rb_tree_minimum(struct rb_tree *root, struct rb_node *x);

/* Returns the entry with the smallest key in the tree, or NULL if the tree is empty. */
#define rb_first(tree, type, rb_node_member) ((tree)->root ? container_of(rb_tree_minimum((tree), (tree)->root), type, rb_node_member) : NULL)
