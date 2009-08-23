#include <stdio.h>
#include "tree.h"

int
main(int argc, char **argv)
{
	size_t max_path_len = 0;
	int max_depth = 0;
	const char *type;
	struct tree *t;
	int visit, level = 0;
	char spaces[512];

	(void)argc; /* UNUSED */
	while(*++argv) {
		t = tree_open(*argv);
		tree_dump(t, stdout);
		spaces[0] = '\0';
		while ((visit = tree_next(t))) {
			switch (visit) {
			case TREE_POSTASCENT:
				type = "post-ascent";
				level--;
				spaces[level] = '\0';
				break;
			case TREE_POSTDESCENT:
				type = "post-descent";
				spaces[level] = ' ';
				level++;
				spaces[level] = '\0';
				break;
			case TREE_ERROR_DIR:
				type = "error-dir";
				break;
			case TREE_ERROR_FATAL:
				type = "error-fatal";
				break;
			case TREE_REGULAR:
				type = "";
				break;
			default: type="??"; break;
			}
			if (tree_current_pathlen(t) > max_path_len)
				max_path_len = tree_current_pathlen(t);
			if (level != tree_current_depth(t))
				printf("*******");
			if (tree_current_depth(t) > max_depth)
				max_depth = tree_current_depth(t);
			printf("%s%s (%s)\n", spaces, tree_current_path(t), type);
			if (tree_current_is_physical_dir(t))
				tree_descend(t);
			tree_dump(t, stdout);
		}
		tree_close(t);
		printf("Max path length: %d\n", max_path_len);
		printf("Max depth: %d\n", max_depth);
		fflush(stdout);
	}
	return (0);
}
