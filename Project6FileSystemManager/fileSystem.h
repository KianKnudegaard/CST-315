#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#define MAX_NAME_LEN 64
#define MAX_CHILDREN 64

typedef enum { FILE_NODE, DIR_NODE } NodeType;

typedef struct FSNode {
    char name[MAX_NAME_LEN];
    NodeType type;
    struct FSNode *parent;

    // For files
    char *data;
    int size;

    // For directories
    struct FSNode *children[MAX_CHILDREN];
    int child_count;
} FSNode;

// Global root and current directory
extern FSNode *fs_root;
extern FSNode *current_dir;

// Core operations
void init_file_system();
void fs_rename(const char *old_name, const char *new_name);
void fs_edit_file(const char *name, const char *new_content);
void free_node(FSNode *node);
FSNode *create_node(const char *name, NodeType type, int size);
void add_child(FSNode *parent, FSNode *child);
FSNode *find_node(FSNode *start, const char *name);
void print_tree(FSNode *node, int depth);

// Command handlers
void fs_mkdir(const char *name);
void fs_rmdir(const char *name, int force);
void fs_create_file(const char *name, int size);
void fs_delete_file(const char *name);
void fs_ls();
void fs_tree();
void fs_info(const char *name, int detailed);
void fs_cd(const char *name);
void fs_pwd();
void fs_move(const char *src, const char *dest);
void fs_copy(const char *src, const char *dest);
void fs_copy_dir(const char *src, const char *dest);
FSNode *fs_search(FSNode *start, const char *name);

#endif
