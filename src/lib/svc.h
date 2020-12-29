#ifndef svc_h
#define svc_h

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef struct resolution {
    // NOTE: DO NOT MODIFY THIS STRUCT
    char *file_name;
    char *resolved_file;
} resolution;

struct commit_id_container {
	char* name;
	int change_type;
} commit_id_container;

struct file_obj {
	int hash;
	char* name;
	char* path;
} file_obj;

struct change_file {
	int new_hash;
	int previous_hash;
	char* name;
} change_file;

struct commit {
	
	int n_files;
	int n_added_files;
	int n_changed_files;
	int n_removed_files;
	char* id;
	char* message;
	char* location;
	char** added_files;
	char** removed_files;
	struct change_file* changed_files;
	struct file_obj* files;
} commit;

struct branch {
	int head_index;
	int n_commits;
	int n_tracked_files;
	int up_to_date; // 1 = up to date, 0 = behind
	char* directory; 
	char* name;
	struct commit* commits;
	struct file_obj* tracked_files;
} branch;

struct helper {
	int curr_branch_index;
	int n_branches;
	struct branch* current_branch;
	struct branch* branches;
	
} helper;

void *svc_init(void);

void cleanup(void *helper);

int hash_file(void *helper, char *file_path);

char *svc_commit(void *helper, char *message);

void *get_commit(void *helper, char *commit_id);

char **get_prev_commits(void *helper, void *commit, int *n_prev);

void print_commit(void *helper, char *commit_id);

int svc_branch(void *helper, char *branch_name);

int svc_checkout(void *helper, char *branch_name);

char **list_branches(void *helper, int *n_branches);

int svc_add(void *helper, char *file_name);

int svc_rm(void *helper, char *file_name);

int svc_reset(void *helper, char *commit_id);

char *svc_merge(void *helper, char *branch_name, resolution *resolutions, int n_resolutions);

#endif
