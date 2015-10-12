/* The authors of this work have released all rights to it and placed it
   in the public domain under the Creative Commons CC0 1.0 waiver
   (http://creativecommons.org/publicdomain/zero/1.0/).

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
   IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
   CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
   TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

   Retrieved from: http://en.literateprograms.org/Red-black_tree_(C)?oldid=19567
*/

#include "cver_rbtree.h"

#ifdef CVER_RBTREE_STANDALONE
#include <stdlib.h>
#endif

typedef rbtree_node node;
typedef enum rbtree_node_color color;

static node grandparent(node n);
static node sibling(node n);
static node uncle(node n);
static color node_color(node n);

#ifdef VERIFY_RBTREE
static void verify_properties(rbtree t);
static void verify_property_1(node root);
static void verify_property_2(node root);
static void verify_property_4(node root);
static void verify_property_5(node root);
static void verify_property_5_helper(node n, int black_count, int* black_count_path);
#endif // VERIFY_RBTREE

static node new_node(KEY key, void* value, color node_color, node left, node right);
static node lookup_node(rbtree t, KEY key);
static node lookup_node_range(rbtree t, uptr addr);
static void rotate_left(rbtree t, node n);
static void rotate_right(rbtree t, node n);

static void replace_node(rbtree t, node oldn, node newn);
static void insert_case1(rbtree t, node n);
static void insert_case2(rbtree t, node n);
static void insert_case3(rbtree t, node n);
static void insert_case4(rbtree t, node n);
static void insert_case5(rbtree t, node n);
static node maximum_node(node root);
static void delete_case1(rbtree t, node n);
static void delete_case2(rbtree t, node n);
static void delete_case3(rbtree t, node n);
static void delete_case4(rbtree t, node n);
static void delete_case5(rbtree t, node n);
static void delete_case6(rbtree t, node n);

// static int compare_obj(KEY left, KEY right);
int compare_obj(KEY leftp, KEY rightp) {
  uptr left = leftp.addr;
  uptr right = rightp.addr;
  
  if (left < right) 
    return -1;
  else if (left > right)
    return 1;
  else {
    rbtree_assert(left == right);
    return 0;
  }
}

int compare_obj_range(uptr left, KEY rightp) {
  uptr right = rightp.addr;
  uptr rightSize = rightp.size;
  
  if (left < right) 
    return -1;
  else if (left >= right + rightSize)
    return 1;
  else {
    rbtree_assert(left >= right && left < right + rightSize);
    return 0;
  }
}

node grandparent(node n) {
  rbtree_assert(n != NULL);
  rbtree_assert(n->parent != NULL); /* Not the root node */
  rbtree_assert(n->parent->parent != NULL); /* Not child of root */
  return n->parent->parent;
}
node sibling(node n) {
  rbtree_assert(n != NULL);
  rbtree_assert(n->parent != NULL); /* Root node has no sibling */
  if (n == n->parent->left)
    return n->parent->right;
  else
    return n->parent->left;
}
node uncle(node n) {
  rbtree_assert(n != NULL);
  rbtree_assert(n->parent != NULL); /* Root node has no uncle */
  rbtree_assert(n->parent->parent != NULL); /* Children of root have no uncle */
  return sibling(n->parent);
}

color node_color(node n) {
  return n == NULL ? BLACK : n->color;
}

void verify_properties(rbtree t) {
#ifdef VERIFY_RBTREE
  verify_property_1(t->root);
  verify_property_2(t->root);
  /* Property 3 is implicit */
  verify_property_4(t->root);
  verify_property_5(t->root);
#endif
}

#ifdef VERIFY_RBTREE
void verify_property_1(node n) {
  rbtree_assert(node_color(n) == RED || node_color(n) == BLACK);
  if (n == NULL) return;
  verify_property_1(n->left);
  verify_property_1(n->right);
}
void verify_property_2(node root) {
  rbtree_assert(node_color(root) == BLACK);
}
void verify_property_4(node n) {
  if (node_color(n) == RED) {
    rbtree_assert(node_color(n->left)   == BLACK);
    rbtree_assert(node_color(n->right)  == BLACK);
    rbtree_assert(node_color(n->parent) == BLACK);
  }
  if (n == NULL) return;
  verify_property_4(n->left);
  verify_property_4(n->right);
}
void verify_property_5(node root) {
  int black_count_path = -1;
  verify_property_5_helper(root, 0, &black_count_path);
}

void verify_property_5_helper(node n, int black_count, int* path_black_count) {
  if (node_color(n) == BLACK) {
    black_count++;
  }
  if (n == NULL) {
    if (*path_black_count == -1) {
      *path_black_count = black_count;
    } else {
      rbtree_assert(black_count == *path_black_count);
    }
    return;
  }
  verify_property_5_helper(n->left,  black_count, path_black_count);
  verify_property_5_helper(n->right, black_count, path_black_count);
}
#endif // VERIFY_RBTREE

rbtree __cver::rbtree_create() {
  rbtree t = (rbtree)rbtree_malloc(sizeof(rbtree_t));
  t->root = NULL;
  verify_properties(t);
  return t;
}

node new_node(KEY key, void* value, color node_color, node left, node right) {
  node result = (node)rbtree_malloc(sizeof(rbtree_node_t));
  result->key = key;
  result->value = value;
  result->color = node_color;
  result->left = left;
  result->right = right;
  if (left  != NULL)  left->parent = result;
  if (right != NULL) right->parent = result;
  result->parent = NULL;
  return result;
}
node lookup_node(rbtree t, KEY key) {
  node n = t->root;
  while (n != NULL) {
    int comp_result = compare_obj(key, n->key);
    if (comp_result == 0) {
      return n;
    } else if (comp_result < 0) {
      n = n->left;
    } else {
      rbtree_assert(comp_result > 0);
      n = n->right;
    }
  }
  return n;
}

node lookup_node_range(rbtree t, uptr addr) {
  node n = t->root;
  while (n != NULL) {
    int comp_result = compare_obj_range(addr, n->key);
    if (comp_result == 0) {
      return n;
    } else if (comp_result < 0) {
      n = n->left;
    } else {
      rbtree_assert(comp_result > 0);
      n = n->right;
    }
  }
  return n;
}

void* __cver::rbtree_lookup(rbtree t, KEY key) {
  node n = lookup_node(t, key);
  return n == NULL ? NULL : n->value;
}

void* __cver::rbtree_lookup_range(rbtree t, uptr addr, uptr *baseAddr) {
  node n = lookup_node_range(t, addr);
  if (!n)
    return NULL;
  *baseAddr = n->key.addr;
  return n->value;
}

void rotate_left(rbtree t, node n) {
  node r = n->right;
  replace_node(t, n, r);
  n->right = r->left;
  if (r->left != NULL) {
    r->left->parent = n;
  }
  r->left = n;
  n->parent = r;
}

void rotate_right(rbtree t, node n) {
  node L = n->left;
  replace_node(t, n, L);
  n->left = L->right;
  if (L->right != NULL) {
    L->right->parent = n;
  }
  L->right = n;
  n->parent = L;
}
void replace_node(rbtree t, node oldn, node newn) {
  if (oldn->parent == NULL) {
    t->root = newn;
  } else {
    if (oldn == oldn->parent->left)
      oldn->parent->left = newn;
    else
      oldn->parent->right = newn;
  }
  if (newn != NULL) {
    newn->parent = oldn->parent;
  }
}
void __cver::rbtree_insert(rbtree t, KEY key, void* value) {
  node inserted_node = new_node(key, value, RED, NULL, NULL);
  if (t->root == NULL) {
    t->root = inserted_node;
  } else {
    node n = t->root;
    while (1) {
      int comp_result = compare_obj(key, n->key);
      if (comp_result == 0) {
        n->value = value;
        return;
      } else if (comp_result < 0) {
        if (n->left == NULL) {
          n->left = inserted_node;
          break;
        } else {
          n = n->left;
        }
      } else {
        rbtree_assert(comp_result > 0);
        if (n->right == NULL) {
          n->right = inserted_node;
          break;
        } else {
          n = n->right;
        }
      }
    }
    inserted_node->parent = n;
  }
  insert_case1(t, inserted_node);
  verify_properties(t);
}
void insert_case1(rbtree t, node n) {
  if (n->parent == NULL)
    n->color = BLACK;
  else
    insert_case2(t, n);
}
void insert_case2(rbtree t, node n) {
  if (node_color(n->parent) == BLACK)
    return; /* Tree is still valid */
  else
    insert_case3(t, n);
}
void insert_case3(rbtree t, node n) {
  if (node_color(uncle(n)) == RED) {
    n->parent->color = BLACK;
    uncle(n)->color = BLACK;
    grandparent(n)->color = RED;
    insert_case1(t, grandparent(n));
  } else {
    insert_case4(t, n);
  }
}
void insert_case4(rbtree t, node n) {
  if (n == n->parent->right && n->parent == grandparent(n)->left) {
    rotate_left(t, n->parent);
    n = n->left;
  } else if (n == n->parent->left && n->parent == grandparent(n)->right) {
    rotate_right(t, n->parent);
    n = n->right;
  }
  insert_case5(t, n);
}
void insert_case5(rbtree t, node n) {
  n->parent->color = BLACK;
  grandparent(n)->color = RED;
  if (n == n->parent->left && n->parent == grandparent(n)->left) {
    rotate_right(t, grandparent(n));
  } else {
    rbtree_assert(n == n->parent->right && n->parent == grandparent(n)->right);
    rotate_left(t, grandparent(n));
  }
}
void __cver::rbtree_delete(rbtree t, KEY key) {
  node child;
  node n = lookup_node(t, key);
  if (n == NULL) return;  /* Key not found, do nothing */
  if (n->left != NULL && n->right != NULL) {
    /* Copy key/value from predecessor and then delete it instead */
    node pred = maximum_node(n->left);
    n->key   = pred->key;
    n->value = pred->value;
    n = pred;
  }

  rbtree_assert(n->left == NULL || n->right == NULL);
  child = n->right == NULL ? n->left  : n->right;
  if (node_color(n) == BLACK) {
    n->color = node_color(child);
    delete_case1(t, n);
  }
  replace_node(t, n, child);
  if (n->parent == NULL && child != NULL) // root should be black
    child->color = BLACK;
  rbtree_free(n);

  verify_properties(t);
}
static node maximum_node(node n) {
  rbtree_assert(n != NULL);
  while (n->right != NULL) {
    n = n->right;
  }
  return n;
}
void delete_case1(rbtree t, node n) {
  if (n->parent == NULL)
    return;
  else
    delete_case2(t, n);
}
void delete_case2(rbtree t, node n) {
  if (node_color(sibling(n)) == RED) {
    n->parent->color = RED;
    sibling(n)->color = BLACK;
    if (n == n->parent->left)
      rotate_left(t, n->parent);
    else
      rotate_right(t, n->parent);
  }
  delete_case3(t, n);
}
void delete_case3(rbtree t, node n) {
  if (node_color(n->parent) == BLACK &&
      node_color(sibling(n)) == BLACK &&
      node_color(sibling(n)->left) == BLACK &&
      node_color(sibling(n)->right) == BLACK)
    {
      sibling(n)->color = RED;
      delete_case1(t, n->parent);
    }
  else
    delete_case4(t, n);
}
void delete_case4(rbtree t, node n) {
  if (node_color(n->parent) == RED &&
      node_color(sibling(n)) == BLACK &&
      node_color(sibling(n)->left) == BLACK &&
      node_color(sibling(n)->right) == BLACK)
    {
      sibling(n)->color = RED;
      n->parent->color = BLACK;
    }
  else
    delete_case5(t, n);
}
void delete_case5(rbtree t, node n) {
  if (n == n->parent->left &&
      node_color(sibling(n)) == BLACK &&
      node_color(sibling(n)->left) == RED &&
      node_color(sibling(n)->right) == BLACK)
    {
      sibling(n)->color = RED;
      sibling(n)->left->color = BLACK;
      rotate_right(t, sibling(n));
    }
  else if (n == n->parent->right &&
           node_color(sibling(n)) == BLACK &&
           node_color(sibling(n)->right) == RED &&
           node_color(sibling(n)->left) == BLACK)
    {
      sibling(n)->color = RED;
      sibling(n)->right->color = BLACK;
      rotate_left(t, sibling(n));
    }
  delete_case6(t, n);
}
void delete_case6(rbtree t, node n) {
  sibling(n)->color = node_color(n->parent);
  n->parent->color = BLACK;
  if (n == n->parent->left) {
    rbtree_assert(node_color(sibling(n)->right) == RED);
    sibling(n)->right->color = BLACK;
    rotate_left(t, n->parent);
  }
  else
    {
      rbtree_assert(node_color(sibling(n)->left) == RED);
      sibling(n)->left->color = BLACK;
      rotate_right(t, n->parent);
    }
}
