#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fileSystem.h"

FSNode *fs_root = NULL;
FSNode *current_dir = NULL;

void init_file_system() {
    fs_root = create_node("/", DIR_NODE, 0);
    fs_root->parent = fs_root; // root points to itself
    current_dir = fs_root;
}

void fs_rename(const char *old_name, const char *new_name) {
    FSNode *node = find_node(current_dir, old_name);
    if (!node) {
        printf("Node not found.\n");
        return;
    }
    strncpy(node->name, new_name, MAX_NAME_LEN);
    printf("Renamed '%s' to '%s'\n", old_name, new_name);
}

void fs_edit_file(const char *name, const char *new_content) {
    FSNode *file = find_node(current_dir, name);
    if (!file || file->type != FILE_NODE) {
        printf("File not found.\n");
        return;
    }
    free(file->data);
    file->size = strlen(new_content);
    file->data = malloc(file->size);
    strcpy(file->data, new_content);
    printf("File '%s' edited.\n", name);
}

void free_node(FSNode *node) {
    if (node->type == DIR_NODE) {
        for (int i = 0; i < node->child_count; i++) {
            free_node(node->children[i]);
        }
    }
    free(node->data);
    free(node);
}

FSNode *create_node(const char *name, NodeType type, int size) {
    FSNode *node = (FSNode *)malloc(sizeof(FSNode));
    strncpy(node->name, name, MAX_NAME_LEN);
    node->type = type;
    node->parent = NULL;
    node->child_count = 0;
    node->data = NULL;
    node->size = size;
    if (type == FILE_NODE && size > 0) {
        node->data = (char *)malloc(size);
        for (int i = 0; i < size; i++) {
            node->data[i] = 'A' + (rand() % 26);
        }
    }
    return node;
}

void add_child(FSNode *parent, FSNode *child) {
    if (parent->child_count < MAX_CHILDREN) {
        parent->children[parent->child_count++] = child;
        child->parent = parent;
    } else {
        printf("Too many children in directory: %s\n", parent->name);
    }
}

FSNode *find_node(FSNode *start, const char *name) {
    for (int i = 0; i < start->child_count; i++) {
        if (strcmp(start->children[i]->name, name) == 0) {
            return start->children[i];
        }
    }
    return NULL;
}

void fs_mkdir(const char *name) {
    if (!name) return;
    if (find_node(current_dir, name)) {
        printf("Directory already exists.\n");
        return;
    }
    FSNode *dir = create_node(name, DIR_NODE, 0);
    add_child(current_dir, dir);
    printf("Directory '%s' created.\n", name);
}

void fs_rmdir(const char *name, int force) {
    FSNode *dir = find_node(current_dir, name);
    if (!dir || dir->type != DIR_NODE) {
        printf("Directory not found.\n");
        return;
    }
    if (dir->child_count > 0 && !force) {
        printf("Directory not empty. Use -f to force delete.\n");
        return;
    }
    for (int i = 0; i < current_dir->child_count; i++) {
        if (current_dir->children[i] == dir) {
            for (int j = i; j < current_dir->child_count - 1; j++) {
                current_dir->children[j] = current_dir->children[j + 1];
            }
            current_dir->child_count--;
            break;
        }
    }
    free(dir);
    printf("Directory '%s' deleted.\n", name);
}

void fs_create_file(const char *name, int size) {
    if (find_node(current_dir, name)) {
        printf("File already exists.\n");
        return;
    }
    FSNode *file = create_node(name, FILE_NODE, size);
    add_child(current_dir, file);
    printf("File '%s' created with %d bytes.\n", name, size);
}

void fs_delete_file(const char *name) {
    FSNode *file = find_node(current_dir, name);
    if (!file || file->type != FILE_NODE) {
        printf("File not found.\n");
        return;
    }
    for (int i = 0; i < current_dir->child_count; i++) {
        if (current_dir->children[i] == file) {
            for (int j = i; j < current_dir->child_count - 1; j++) {
                current_dir->children[j] = current_dir->children[j + 1];
            }
            current_dir->child_count--;
            break;
        }
    }
    free(file->data);
    free(file);
    printf("File '%s' deleted.\n", name);
}

void fs_ls() {
    for (int i = 0; i < current_dir->child_count; i++) {
        FSNode *child = current_dir->children[i];
        printf("%s\t%s\n", child->type == DIR_NODE ? "[DIR]" : "[FILE]", child->name);
    }
}

void print_tree(FSNode *node, int depth) {
    for (int i = 0; i < depth; i++) printf("  ");
    printf("%s %s\n", node->type == DIR_NODE ? "[DIR]" : "[FILE]", node->name);
    if (node->type == DIR_NODE) {
        for (int i = 0; i < node->child_count; i++) {
            print_tree(node->children[i], depth + 1);
        }
    }
}

void fs_tree() {
    print_tree(current_dir, 0);
}

void fs_info(const char *name, int detailed) {
    FSNode *node = find_node(current_dir, name);
    if (!node) {
        printf("Node not found.\n");
        return;
    }
    printf("Name: %s\n", node->name);
    printf("Type: %s\n", node->type == DIR_NODE ? "Directory" : "File");
    if (node->type == FILE_NODE) {
        printf("Size: %d\n", node->size);
        if (detailed && node->data) {
            printf("Data: %.32s...\n", node->data);
        }
    } else if (detailed) {
        printf("Children: %d\n", node->child_count);
    }
}

void fs_cd(const char *name) {
    if (strcmp(name, "..") == 0) {
        current_dir = current_dir->parent;
        return;
    }
    FSNode *dir = find_node(current_dir, name);
    if (dir && dir->type == DIR_NODE) {
        current_dir = dir;
    } else {
        printf("Directory not found.\n");
    }
}

void fs_pwd() {
    FSNode *node = current_dir;
    char path[1024] = "";
    while (node != fs_root) {
        char temp[1024];
        sprintf(temp, "/%s%s", node->name, path);
        strcpy(path, temp);
        node = node->parent;
    }
    printf("/%s\n", path);
}

void fs_move(const char *src, const char *dest) {
    FSNode *node = find_node(current_dir, src);
    if (!node) {
        printf("Source not found.\n");
        return;
    }
    FSNode *target_dir = find_node(current_dir, dest);
    if (!target_dir || target_dir->type != DIR_NODE) {
        printf("Destination directory not found.\n");
        return;
    }
    add_child(target_dir, node);
    // Remove from current_dir
    for (int i = 0; i < current_dir->child_count; i++) {
        if (current_dir->children[i] == node) {
            for (int j = i; j < current_dir->child_count - 1; j++) {
                current_dir->children[j] = current_dir->children[j + 1];
            }
            current_dir->child_count--;
            break;
        }
    }
    printf("Moved '%s' to '%s'.\n", src, dest);
}

FSNode *copy_node(FSNode *src) {
    FSNode *copy = create_node(src->name, src->type, src->size);
    if (src->type == FILE_NODE && src->data) {
        memcpy(copy->data, src->data, src->size);
    } else {
        for (int i = 0; i < src->child_count; i++) {
            FSNode *child_copy = copy_node(src->children[i]);
            add_child(copy, child_copy);
        }
    }
    return copy;
}

void fs_copy(const char *src, const char *dest) {
    FSNode *src_node = find_node(current_dir, src);
    if (!src_node) {
        printf("Source not found.\n");
        return;
    }
    FSNode *copy = copy_node(src_node);
    strncpy(copy->name, dest, MAX_NAME_LEN);
    add_child(current_dir, copy);
    printf("Copied '%s' to '%s'.\n", src, dest);
}

void fs_copy_dir(const char *src, const char *dest) {
    FSNode *src_node = find_node(current_dir, src);
    if (!src_node || src_node->type != DIR_NODE) {
        printf("Source directory not found.\n");
        return;
    }
    FSNode *copy = copy_node(src_node);
    strncpy(copy->name, dest, MAX_NAME_LEN);
    add_child(current_dir, copy);
    printf("Directory '%s' duplicated as '%s'.\n", src, dest);
}

FSNode *fs_search(FSNode *start, const char *name) {
    if (strcmp(start->name, name) == 0) return start;
    for (int i = 0; i < start->child_count; i++) {
        FSNode *res = fs_search(start->children[i], name);
        if (res) return res;
    }
    return NULL;
}