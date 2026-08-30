#include <string.h>
#include <rbtree.h>

/**
 * rb_left_rotate - Perform a left rotation on a red-black tree node
 *
 * This function rotates the subtree rooted at x to the left. After the rotation,
 * x's right child (y) becomes the new root of the subtree, and x becomes y's
 * left child. The size fields are updated to maintain the augmentation.
 *
 * Before:        After:
 *     x            y
 *    / \          / \
 *   a   y   =>   x   c
 *      / \      / \
 *     b   c    a   b
 *
 * @root: The tree containing node x
 * @x: The node to rotate (must have a non-NULL right child)
 */
void rb_left_rotate(struct rb_tree *root, struct rb_node *x)
{
	struct rb_node *y = x->right;

	/* Turn y's left subtree into x's right subtree */
	x->right = y->left;

	/* If y's left subtree isn't null, set its parent to x */
	if (y->left != NULL) {
		y->left->parent = x;
	}

	/* Link x's parent to y */
	y->parent = x->parent;

	/* If x's parent is null, y becomes the new root (since x was the root) */
	if (x->parent == NULL) {
		root->root = y;
	} else if (x == x->parent->left) {
		/* If x was the left child of its parent, y becomes the left child */
		x->parent->left = y;
	} else {
		/* If x was the right child of its parent, y becomes the right child */
		x->parent->right = y;
	}

	/* Put x on y's left */
	y->left = x;
	x->parent = y;

	/* Maintain the size augmentation (CLRS page 484) */
	y->size = x->size;
	x->size = (x->left ? x->left->size : 0) + (x->right ? x->right->size : 0) + 1;
}

/**
 * rb_right_rotate - Perform a right rotation on a red-black tree node
 *
 * This function rotates the subtree rooted at x to the right. After the rotation,
 * x's left child (y) becomes the new root of the subtree, and x becomes y's
 * right child. The size fields are updated to maintain the augmentation.
 *
 * Before:      After:
 *     x          y
 *    / \        / \
 *   y   c  =>  a   x
 *  / \            / \
 * a   b          b   c
 *
 * Based on the left rotation with directions reversed.
 *
 * @root: The tree containing node x
 * @x: The node to rotate (must have a non-NULL left child)
 */
void rb_right_rotate(struct rb_tree *root, struct rb_node *x)
{
	/* Set y to be x's left child */
	struct rb_node *y = x->left;

	/* Turn y's right subtree into x's left subtree */
	x->left = y->right;

	/* If y's right subtree isn't null, set its parent to x */
	if (y->right != NULL) {
		y->right->parent = x;
	}

	/* Link x's parent to y */
	y->parent = x->parent;

	/* If x's parent is null, y becomes the new root (since x was the root) */
	if (x->parent == NULL) {
		root->root = y;
	} else if (x == x->parent->right) {
		/* If x was the right child of its parent, y becomes the right child */
		x->parent->right = y;
	} else {
		/* If x was the left child of its parent, y becomes the left child */
		x->parent->left = y;
	}

	/* Put x on y's right */
	y->right = x;
	x->parent = y;

	/* Maintain the size augmentation (CLRS page 484) */
	y->size = x->size;
	x->size = (x->left ? x->left->size : 0) + (x->right ? x->right->size : 0) + 1;
}

/**
 * rb_fix_tree_insert - Restore red-black tree properties after insertion
 *
 * After inserting a new RED node, this function restores the red-black tree
 * properties by recoloring nodes and performing rotations.
 *
 * @root: The tree containing the new node
 * @newNode: The newly inserted RED node
 */
void rb_fix_tree_insert(struct rb_tree *root, struct rb_node *newNode)
{
	/* Recolor nodes and perform rotations to restore red-black properties */
	while (newNode->parent && newNode->parent->color == RB_RED) {
		/* Case: Parent is a left child */
		if (newNode->parent == newNode->parent->parent->left) {
			/* y is the uncle (parent's sibling) */
			struct rb_node *y = newNode->parent->parent->right;
			if (y && y->color == RB_RED) {
				/* Case 1: Uncle is RED - recolor and move up the tree */
				newNode->parent->color = RB_BLACK;
				y->color = RB_BLACK;
				newNode->parent->parent->color = RB_RED;
				newNode = newNode->parent->parent;
			} else {
				/* Case 2 & 3: Uncle is BLACK */
				if (newNode == newNode->parent->right) {
					/* Case 2: Triangle pattern - rotate to make it a line */
					newNode = newNode->parent;
					rb_left_rotate(root, newNode);
				}

				/* Case 3: Line pattern - recolor and rotate */
				newNode->parent->color = RB_BLACK;
				newNode->parent->parent->color = RB_RED;
				rb_right_rotate(root, newNode->parent->parent);
			}
		}
		/* Case: Parent is a right child (symmetric to above) */
		else {
			/* y is the uncle (parent's sibling) */
			struct rb_node *y = newNode->parent->parent->left;
			if (y && y->color == RB_RED) {
				/* Case 1: Uncle is RED - recolor and move up the tree */
				newNode->parent->color = RB_BLACK;
				y->color = RB_BLACK;
				newNode->parent->parent->color = RB_RED;
				newNode = newNode->parent->parent;
			} else {
				/* Case 2 & 3: Uncle is BLACK */
				if (newNode == newNode->parent->left) {
					/* Case 2: Triangle pattern - rotate to make it a line */
					newNode = newNode->parent;
					rb_right_rotate(root, newNode);
				}

				/* Case 3: Line pattern - recolor and rotate */
				newNode->parent->color = RB_BLACK;
				newNode->parent->parent->color = RB_RED;
				rb_left_rotate(root, newNode->parent->parent);
			}
		}
	}

	/* Root must always be BLACK (property 2) */
	root->root->color = RB_BLACK;
}

/**
 * rb_transplant - Replace one subtree with another
 *
 * This is a helper function for deletion that replaces the subtree rooted at
 * node u with the subtree rooted at node v. Used during the deletion process
 * to splice out nodes.
 *
 * @root: The tree containing the nodes
 * @u: The node whose subtree will be replaced
 * @v: The node that will replace u (may be NULL)
 */
void rb_transplant(struct rb_tree *root, struct rb_node *u, struct rb_node *v)
{
	if (u->parent == NULL)
		root->root = v;
	else if (u == u->parent->left)
		u->parent->left = v;
	else
		u->parent->right = v;

	if (v)
		v->parent = u->parent;
}

/**
 * rb_tree_minimum - Find the minimum node in a subtree
 *
 * Returns the node with the smallest key in the subtree rooted at x.
 * This is the leftmost node in the subtree.
 *
 * @root: The tree (unused, kept for API consistency)
 * @x: The root of the subtree to search
 * @return: The minimum node in the subtree
 */
struct rb_node *rb_tree_minimum(struct rb_tree *root, struct rb_node *x)
{
	while (x->left != NULL)
		x = x->left;
	return x;
}

/**
 * rb_tree_maximum - Find the maximum node in a subtree
 *
 * Returns the node with the largest key in the subtree rooted at x.
 * This is the rightmost node in the subtree.
 *
 * @root: The tree (unused, kept for API consistency)
 * @x: The root of the subtree to search
 * @return: The maximum node in the subtree
 */
struct rb_node *rb_tree_maximum(struct rb_tree *root, struct rb_node *x)
{
	while (x->right != NULL)
		x = x->right;
	return x;
}

/**
 * rb_fix_tree_remove - Restore red-black properties after deletion
 *
 * After deleting a BLACK node, this function restores the red-black tree
 * properties by recoloring nodes and performing rotations. The function handles
 * four cases based on the color and structure of the sibling subtree.
 *
 * @root: The tree containing the affected node
 * @nodeToRemove: The node that replaced the deleted node (may have extra black)
 */
void rb_fix_tree_remove(struct rb_tree *root, struct rb_node *nodeToRemove)
{
	while (nodeToRemove != root->root && nodeToRemove->color == RB_BLACK) {
		if (nodeToRemove == nodeToRemove->parent->left) {
			struct rb_node *w = nodeToRemove->parent->right;
			if (w && w->color == RB_RED) {
				// Case 1
				w->color = RB_BLACK;
				nodeToRemove->parent->color = RB_RED;
				rb_left_rotate(root, nodeToRemove->parent);
				w = nodeToRemove->parent->right;
			}
			if (w && (!w->left || w->left->color == RB_BLACK) && (!w->right || w->right->color == RB_BLACK)) {
				// Case 2
				w->color = RB_RED;
				nodeToRemove = nodeToRemove->parent;
			} else if (w) {
				if (!w->right || w->right->color == RB_BLACK) {
					// Case 3
					if (w->left)
						w->left->color = RB_BLACK;
					w->color = RB_RED;
					rb_right_rotate(root, w);
					w = nodeToRemove->parent->right;
				}

				// Case 4
				w->color = nodeToRemove->parent->color;
				nodeToRemove->parent->color = RB_BLACK;
				if (w->right)
					w->right->color = RB_BLACK;
				rb_left_rotate(root, nodeToRemove->parent);
				nodeToRemove = root->root;
			}
		} else {
			struct rb_node *w = nodeToRemove->parent->left;
			if (w && w->color == RB_RED) {
				// Case 1
				w->color = RB_BLACK;
				nodeToRemove->parent->color = RB_RED;
				rb_right_rotate(root, nodeToRemove->parent);
				w = nodeToRemove->parent->left;
			}
			if (w && (!w->right || w->right->color == RB_BLACK) && (!w->left || w->left->color == RB_BLACK)) {
				// Case 2
				w->color = RB_RED;
				nodeToRemove = nodeToRemove->parent;
			} else if (w) {
				if (!w->left || w->left->color == RB_BLACK) {
					// Case 3
					if (w->right)
						w->right->color = RB_BLACK;
					w->color = RB_RED;
					rb_left_rotate(root, w);
					w = nodeToRemove->parent->left;
				}

				// Case 4
				w->color = nodeToRemove->parent->color;
				nodeToRemove->parent->color = RB_BLACK;
				if (w->left)
					w->left->color = RB_BLACK;
				rb_right_rotate(root, nodeToRemove->parent);
				nodeToRemove = root->root;
			}
		}
	}
	nodeToRemove->color = RB_BLACK;
}

/**
 * rb_remove - Remove a node from the red-black tree
 *
 * This function removes a node from the tree while maintaining red-black
 * properties and updating size fields.
 *
 * @root: The tree containing the node
 * @nodeToRemove: The node to remove (NULL is safely ignored)
 */
void rb_remove(struct rb_tree *root, struct rb_node *nodeToRemove)
{
	if (nodeToRemove == NULL)
		return; /* Node doesn't exist */

	/* Update sizes along the path from nodeToRemove to root
    * As described in CLRS (page 485): traverse a simple path from the
    * lowest node that moves up to the root, decrementing the size of each node */

	/* Decrement sizes from parent up to (but not including) root */
	if (nodeToRemove != root->root) {
		struct rb_node *x = nodeToRemove->parent;
		while (x != root->root) {
			x->size--;
			x = x->parent;
		}

		/* Manually decrement the root's size */
		root->root->size--;
	}

	/* Standard BST deletion with red-black fixup */
	struct rb_node *y = nodeToRemove;
	int yOriginalColor = y->color;
	struct rb_node *x;

	if (nodeToRemove->left == NULL) {
		/* Case: No left child - replace with right child */
		x = nodeToRemove->right;
		rb_transplant(root, nodeToRemove, nodeToRemove->right);
	} else if (nodeToRemove->right == NULL) {
		/* Case: No right child - replace with left child */
		x = nodeToRemove->left;
		rb_transplant(root, nodeToRemove, nodeToRemove->left);
	} else {
		/* Case: Two children - find successor and replace */
		y = rb_tree_minimum(root, nodeToRemove->right);
		yOriginalColor = y->color;
		x = y->right;
		if (y != nodeToRemove->right) {
			rb_transplant(root, y, y->right);
			y->right = nodeToRemove->right;
			if (y->right)
				y->right->parent = y;

			/* Update sizes for the moved subtree
             * As described in CLRS: for O(1) rotations in the second phase of deletion,
             * handle them in the same manner as for insertion */
			struct rb_node *t = x ? x->parent : y;
			while (t != y) {
				t->size--;
				t = t->parent;
			}

			/* Update y's size to reflect only its left child */
			y->size = (y->left ? y->left->size : 0) + 1;
		} else {
			if (x)
				x->parent = y;
		}

		rb_transplant(root, nodeToRemove, y);
		y->left = nodeToRemove->left;
		if (y->left)
			y->left->parent = y;
		y->color = nodeToRemove->color;

		/* Update y's final size to include both children */
		y->size = (y->left ? y->left->size : 0) + (y->right ? y->right->size : 0) + 1;
	}

	/* If we removed a BLACK node, fix red-black properties */
	if (yOriginalColor == RB_BLACK && x)
		rb_fix_tree_remove(root, x);

	/* Decrement total tree size */
	root->size--;
}

/**
 * rb_next - Find the in-order successor of a node
 *
 * Returns the node with the next larger key in the tree. This is useful for
 * iterating through the tree in sorted order.
 *
 * Keep in mind that the size augmentation also allows skipping ahead by k elements
 * in O(log n) time using rb_next_k.
 *
 * @root: The tree containing the node
 * @x: The node whose successor to find
 * @return: The successor node, or NULL if x is the maximum node
 */
struct rb_node *rb_next(struct rb_tree *root, struct rb_node *x)
{
	/* If right subtree exists, successor is the leftmost node in it */
	if (x->right != NULL)
		return rb_tree_minimum(root, x->right);

	/* Otherwise, find the lowest ancestor whose left child is also an ancestor of x */
	struct rb_node *y = x->parent;
	while (y != NULL && x == y->right) {
		x = y;
		y = y->parent;
	}
	return y;
}

/**
 * rb_next_k - Find the k-th smallest element in a subtree (1-indexed)
 *
 * This function uses the size augmentation to efficiently find the k-th
 * smallest element in the subtree rooted at x. This is an order-statistic
 * query that runs in O(log n) time.
 *
 * @x: The root of the subtree to search
 * @k: The rank to find (1 = smallest, 2 = second smallest, etc.)
 * @return: The k-th smallest node in the subtree
 */
struct rb_node *rb_next_k(struct rb_node *x, int k)
{
	int r = (x->left ? x->left->size : 0) + 1; /* Rank of x within its subtree */
	if (k == r)
		return x;
	else if (k < r)
		return rb_next_k(x->left, k);
	else
		return rb_next_k(x->right, k - r);
}

/**
 * rb_index_element - Find the k-th smallest element in the tree (0-indexed)
 *
 * This is a wrapper around rb_next_k that provides 0-indexed access to
 * tree elements, similar to array indexing. Returns the element at position k
 * when all elements are sorted.
 *
 * @root: The tree to search
 * @k: The index to find (0 = smallest, 1 = second smallest, etc.)
 * @return: The k-th smallest node, or NULL if k is out of bounds
 */
struct rb_node *rb_index_element(struct rb_tree *root, int k)
{
	if (!root->root || k >= root->root->size)
		return NULL;
	if (k < 0)
		return NULL;

	/* Add 1 to k because rb_next_k uses 1-indexed ranks */
	return rb_next_k(root->root, k + 1);
}
