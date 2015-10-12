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

#ifndef CVER_RBTREE_H
#define CVER_RBTREE_H

#ifdef CVER_RBTREE_STANDALONE
#include <assert.h>
typedef long unsigned int uptr;
#define rbtree_malloc(size) malloc(size)
#define rbtree_free(size) free(size)
#define rbtree_assert(cond) assert(cond)
#else
#include "cver_allocator.h"
#define rbtree_malloc(size) __cver::CverReallocate(0, 0, size, sizeof(u64), false)
#define rbtree_free(ptr) __cver::CverDeallocate(0, ptr)
#define rbtree_assert(cond)
#define NULL 0
#endif // CVER_RBTREE_STANDALONE

struct _KEY {
  uptr addr;
  uptr size;  
};
typedef struct _KEY KEY;
typedef uptr VALUE;

enum rbtree_node_color { RED, BLACK };

struct rbtree_node_t {
    KEY key;
    void* value;
    struct rbtree_node_t* left;
    struct rbtree_node_t* right;
    struct rbtree_node_t* parent;
    enum rbtree_node_color color;
};
typedef rbtree_node_t *rbtree_node;

struct rbtree_t {
    rbtree_node root;
};
typedef rbtree_t *rbtree;

namespace __cver {
rbtree rbtree_create();
void* rbtree_lookup(rbtree t, KEY key);
void* rbtree_lookup_range(rbtree t, uptr addr, uptr *baseAddr);
void rbtree_insert(rbtree t, KEY key, void* value);
void rbtree_delete(rbtree t, KEY key);
} // namespace __cver

#endif // CVER_RBTREE_H
