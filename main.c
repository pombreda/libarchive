#include <stdio.h>
#include "tree.h"

int
main(int argc, char **argv)
{
  size_t max_path_len = 0;
  int max_depth = 0;

  while(*++argv) {
	  const char *type;
    struct tree *t = tree_open(*argv);
	int visit;
    while ((visit = tree_next(t))) {
      size_t path_len = tree_current_pathlen(t);
      int depth = tree_current_depth(t);
      if (path_len > max_path_len)
        max_path_len = path_len;
      if (depth > max_depth)
        max_depth = depth;
	  switch (visit) {
		  case TREE_POSTASCENT: type = "post-ascent"; break;
		  case TREE_POSTDESCENT: type = "post-descent"; break;
		  case TREE_ERROR_DIR: type = "error-dir"; break;
		  case TREE_ERROR_FATAL: type = "error-fatal"; break;
		  case TREE_REGULAR: type = ""; break;
		  default: type="??"; break;
	  }
	  printf("%s: %s\n", type, tree_current_path(t));
      if (tree_current_is_physical_dir(t))
        tree_descend(t);
    }
    tree_close(t);
    printf("Max path length: %d\n", max_path_len);
    printf("Max depth: %d\n", max_depth);
//    printf("Final open count: %d\n", t->openCount);
//    printf("Max open count: %d\n", t->maxOpenCount);
    fflush(stdout);
  }
  return (0);
}
