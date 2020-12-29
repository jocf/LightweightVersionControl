#include "./lib/svc.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
typedef struct helper helper_t;

void *svc_init(void) {
	struct branch* master = malloc(sizeof(struct branch)); // Start off by creating our master branch
	master->n_commits = 0; // Init to 0 commits
	master->name = malloc(sizeof("master") + 1); // Set name to master
    strcpy(master->name, "master");
    master->directory = NULL;
    master->n_tracked_files = 0; // Set number of tracked files to 0
    master->up_to_date = 1;
	
	struct helper* helper = malloc(sizeof(struct helper)); // Create our helper struct
	helper->n_branches = 1; // Init to 1 branch (the master branch)
	helper->current_branch = master;
	/*
        We start at "master" and then increment each branch from here. This will remain constant,
	    while the current_branch will change depending on the checked-out branch.
    */
	helper->branches = master;
    helper->curr_branch_index = 0;
    int check;
    check = mkdir("commits",0777);
    if(check){
        printf("Commits directory already exists!\n");
    }
    return (struct helper*)helper;
}

void cleanup(void *helper) {
    helper_t* temp_helper = helper;
    /* 
        Modularised cleanup method.
        Loop through branch array and clean each component
        In each branch
    */
    for(int i = 0; i < temp_helper->n_branches; i++){
        // Clear tracked files from the branch.
        for(int j = 0; j < temp_helper->branches[i].n_tracked_files; j++){
            free(temp_helper->branches[i].tracked_files[j].path);
            free(temp_helper->branches[i].tracked_files[j].name);
        }
        if(temp_helper->branches[i].n_tracked_files > 0){
            free(temp_helper->branches[i].tracked_files);
        }
        // Now we need to clean the commits
        if(temp_helper->branches[i].n_commits != 0){
            for(int k = 0; k < temp_helper->branches[i].n_commits; k++){
                for(int l = 0; l < temp_helper->branches[i].commits[k].n_files; l++){
                    free(temp_helper->branches[i].commits[k].files[l].name);
                    free(temp_helper->branches[i].commits[k].files[l].path);
                }
                free(temp_helper->branches[i].commits[k].files);
                free(temp_helper->branches[i].commits[k].message);
                free(temp_helper->branches[i].commits[k].id);
                free(temp_helper->branches[i].commits[k].location);
                for(int m = 0; m < temp_helper->branches[i].commits[k].n_added_files; m++){
                    free(temp_helper->branches[i].commits[k].added_files[m]);
                }
                free(temp_helper->branches[i].commits[k].added_files);
                for(int n = 0; n < temp_helper->branches[i].commits[k].n_removed_files; n++){
                    free(temp_helper->branches[i].commits[k].removed_files[n]);
                }
                free(temp_helper->branches[i].commits[k].removed_files);
                for(int o = 0; o < temp_helper->branches[i].commits[k].n_changed_files; o++){
                    free(temp_helper->branches[i].commits[k].changed_files[o].name);
                    free(&(temp_helper->branches[i].commits[k].changed_files[o]));
                }
                if(temp_helper->branches[i].commits[k].n_changed_files == 0){
                    free((temp_helper->branches[i].commits[k].changed_files));
                }
            }
            free(temp_helper->branches[i].commits);
        }
        free(temp_helper->branches[i].directory);
        free(temp_helper->branches[i].name);
    }
    
    free(temp_helper->branches);
    free(helper);
    
}

int hash_file(void *helper, char *file_path) {
    FILE *file = fopen(file_path, "r");
    if(file_path == NULL){
        return -1;
    }
	if(file == NULL){
		return -2;
	}else{
		int hash = 0;
		for(int i = 0; i < strlen(file_path); i++){
			hash = (hash + file_path[i]) % 1000;
		}
		int char_int;
		while ((char_int = getc(file)) != EOF){
			hash = (hash + char_int) % 2000000000;
		}
		return hash;
	}
}

void copy_files(char* dest, struct branch* branch){
    for(int i = 0; i < branch->n_tracked_files; i++){
        FILE * source;
        FILE * destination;

        char str[100];
        sprintf(str, "%d", branch->tracked_files[i].hash);
        char path[strlen(dest) + strlen(str) + 2];
        strcpy(path,dest);
        strcat(path,"/");
        strcat(path,str);
        
        source = fopen(branch->tracked_files[i].path, "r");
        destination = fopen(path, "w");
        if(source == NULL){
            return;
        }
        if(destination == NULL){
            return;
        }
        int ch; // Use int and then typecast to char to deal with non standard characters
        while((ch = fgetc(source)) != EOF){
            fputc((char)ch, destination);
        }
        fclose(source);
        fclose(destination);
    }
    return;
}

/* 
    Method adapted from http://www.anyexample.com/programming/c/qsort__sorting_array_of_strings__integers_and_structs.xml
    Advice to use qsort was provided on Edstem
    Sort file_objs by name
*/

int alphabetise(const void *a, const void *b) 
{ 
    struct file_obj *file_a = (struct file_obj *)a;
    struct file_obj *file_b = (struct file_obj *)b;
    
    return strcasecmp(file_a->name, file_b->name);
} 

/* 
    Method adapted from http://www.anyexample.com/programming/c/qsort__sorting_array_of_strings__integers_and_structs.xml
    Advice to use qsort was provided on Edstem
    Sort commit_id_containers by name
*/

int alphabetise_b(const void *a, const void *b) 
{ 
    struct commit_id_container *file_a = (struct commit_id_container *)a;
    struct commit_id_container *file_b = (struct commit_id_container *)b;
    
    return strcasecmp(file_a->name, file_b->name);
} 

/* 
    Method adapted from http://www.anyexample.com/programming/c/qsort__sorting_array_of_strings__integers_and_structs.xml
    Advice to use qsort was provided on Edstem
    Sort file_objs by hash
*/

int int_sort(const void *a, const void *b) 
{ 
    struct file_obj *int_a = (struct file_obj *)a;
    struct file_obj *int_b = (struct file_obj *)b;
    
    return int_a->hash - int_b->hash; 
}

void check_deletion(void *helper){
    /*
        Used to detect manual deletions before a commit is made
        Loop through each file, attempt to open it, if it fails, delete the file with rm and move on.
    */
    helper_t* temp_helper = (helper_t*)helper;
    for(int i = 0; i < temp_helper->current_branch->n_tracked_files; i++){
        FILE* file = fopen(temp_helper->current_branch->tracked_files[i].path, "r");
        if(file == NULL){
            svc_rm(helper,temp_helper->current_branch->tracked_files[i].name);
            temp_helper->current_branch->up_to_date = 0;
        }
    }
}

void rehash_files(void *helper){
    /*
        Used to rehash files before a commit is made in the case of manual editing
    */
    helper_t* temp_helper = (helper_t*)helper;
    for(int i = 0; i < temp_helper->current_branch->n_tracked_files; i++){
        int temp_hash = temp_helper->current_branch->tracked_files[i].hash;
        temp_helper->current_branch->tracked_files[i].hash = 
            hash_file(helper,temp_helper->current_branch->tracked_files[i].name);
        
        if(temp_hash != temp_helper->current_branch->tracked_files[i].hash){
            temp_helper->current_branch->up_to_date = 0;
        }
    }
}

void get_commit_id(void *helper, char *message, char* temp_hex) {
    helper_t* temp_helper = (helper_t*)helper;
    // First ID calculation. Treat every file as an add.
    if(temp_helper->current_branch->n_commits == 1){
        int id = 0;
		for(int i = 0; i < strlen(message); i++){
			id = (id + message[i]) % 1000;
		}        
        // Now we will sort the files alphabetically
        qsort(temp_helper->current_branch->tracked_files, temp_helper->current_branch->n_tracked_files, 
              sizeof(struct file_obj), alphabetise);  
        
        for(int j = 0; j < temp_helper->current_branch->n_tracked_files; j++){
            id += 376591;
            // Adding to current commits info:           
            temp_helper->current_branch->commits[temp_helper->current_branch->head_index].n_added_files++;
            struct commit* tmp_cmt = &(temp_helper->current_branch->commits[temp_helper->current_branch->head_index]);
            int temp_n = temp_helper->current_branch->commits[temp_helper->current_branch->head_index].n_added_files;
            
            tmp_cmt->added_files = realloc(tmp_cmt->added_files,temp_n*sizeof(char*));
            tmp_cmt->added_files[temp_n - 1] = malloc(strlen(temp_helper->current_branch->tracked_files[j].name) + 1);
            tmp_cmt->added_files[temp_n - 1] = strcpy(tmp_cmt->added_files[temp_n - 1],temp_helper->current_branch->tracked_files[j].name);
            
            for(int k = 0; k < strlen(temp_helper->current_branch->tracked_files[j].name); k++){
                id = ((id * (temp_helper->current_branch->tracked_files[j].name[k] % 37)) % 15485863) + 1;
            }
        }
        
        sprintf(temp_hex, "%x", id);
        // String pad if necessary to 6 characters
        if(strlen(temp_hex) < 6){
            int space = 6 - strlen(temp_hex);
            char* new_hex = malloc(sizeof(char)*7);
            sprintf(new_hex,"%0*d%s",space,0,temp_hex);
            temp_hex = strcpy(temp_hex,new_hex);
            free(new_hex);
        }
    // After first ID calculation
    }else{
        int id = 0;
		for(int i = 0; i < strlen(message); i++){
			id = (id + message[i]) % 1000;
		}
        // Now we will sort the files alphabetically
        qsort(temp_helper->current_branch->tracked_files, temp_helper->current_branch->n_tracked_files, sizeof(struct file_obj), alphabetise); 
        struct commit* prev_commit = &(temp_helper->current_branch->commits[temp_helper->current_branch->head_index - 1]);
        qsort(prev_commit->files, prev_commit->n_files, sizeof(struct file_obj), alphabetise); 
        
        struct commit_id_container* container = malloc(sizeof(struct commit_id_container));
        int container_size = 0;
        
        struct commit* tmp_cmt = &(temp_helper->current_branch->commits[temp_helper->current_branch->head_index]);
        for(int i = 0; i < prev_commit->n_files; i++){
            // LOOK FOR DELETIONS
                // Scan file names, if there is a file name in the most recent commit that ISNT
            int check = 0;
            for(int x = 0; x < temp_helper->current_branch->n_tracked_files; x++){
                if(!strcmp(prev_commit->files[i].name,temp_helper->current_branch->tracked_files[x].name)){
                    // A match was found so flag it as not deleted
                    check = 1;
                }
            }
            // if check = 0 then the file at i in the previous commit has been DELETED YO
            if(check == 0){
                container_size++;
                container = realloc(container,sizeof(struct commit_id_container)*container_size);
                container[container_size - 1].name = malloc(strlen(prev_commit->files[i].name) + 1);
                container[container_size - 1].name = strcpy(container[container_size - 1].name,prev_commit->files[i].name); 
                container[container_size - 1].change_type = 0;
                
                // Adding to current commits info:               
                tmp_cmt->n_removed_files++;
                int temp_n = tmp_cmt->n_removed_files;
                
                tmp_cmt->removed_files = realloc(tmp_cmt->removed_files,temp_n*sizeof(char*));
                tmp_cmt->removed_files[temp_n - 1] = malloc(strlen(prev_commit->files[i].name) + 1);
                tmp_cmt->removed_files[temp_n - 1] = strcpy(tmp_cmt->removed_files[temp_n - 1],prev_commit->files[i].name);
                
            }
        }
        for(int i = 0; i < temp_helper->current_branch->n_tracked_files; i++){
            // LOOK FOR ADDTIONS
                // Scan file names, if there is a file name in the tracked files
            int check = 0;
            for(int x = 0; x < prev_commit->n_files; x++){
                if(!strcmp(temp_helper->current_branch->tracked_files[i].name,prev_commit->files[x].name)){
                    // A match was found so flag it as not added
                    check = 1;
                }
            }
            // if check = 0 then the file at i has been added
            if(check == 0){
                container_size++;
                container = realloc(container,sizeof(struct commit_id_container)*container_size);
                container[container_size - 1].name = malloc(strlen(temp_helper->current_branch->tracked_files[i].name) + 1);
                container[container_size - 1].name = strcpy(container[container_size-1].name,temp_helper->current_branch->tracked_files[i].name); 
                container[container_size - 1].change_type = 1;
                
                // Adding to current commits info:
                // This is the commit we need to add to -> temp_helper->current_branch->commits[temp_helper->current_branch->n_commits-1]
                
                tmp_cmt->n_added_files++;
                int temp_n = tmp_cmt->n_added_files;
                
                tmp_cmt->added_files = realloc(
                tmp_cmt->added_files,temp_n*sizeof(char*));
                tmp_cmt->added_files[temp_n - 1] = malloc(strlen(temp_helper->current_branch->tracked_files[i].name) + 1);
                tmp_cmt->added_files[temp_n - 1] = strcpy(tmp_cmt->added_files[temp_n-1],temp_helper->current_branch->tracked_files[i].name);
                
            }
        }
        for(int i = 0; i < temp_helper->current_branch->n_tracked_files; i++){
            // LOOK FOR MODIFICATIONS
                // Scan file names, if the names match but the hash is different
            int check = 0;
            int old_hash = 0;
            for(int x = 0; x < prev_commit->n_files; x++){
                if(!strcmp(temp_helper->current_branch->tracked_files[i].name,prev_commit->files[x].name)){
                    // Matching file, now compare the hash.
                    if(temp_helper->current_branch->tracked_files[i].hash != prev_commit->files[x].hash){
                        // Mis matched hash so MODIFICATION has been detected.
                        check = 1;
                        old_hash = prev_commit->files[x].hash;
                    }
                    
                }
            }
            // if check = 1 then the file at i has been MODIFIED
            if(check == 1){
                container_size++;
                container = realloc(container,sizeof(struct commit_id_container)*container_size);
                container[container_size - 1].name = malloc(strlen(temp_helper->current_branch->tracked_files[i].name) + 1);
                container[container_size - 1].name = strcpy(container[container_size - 1].name,temp_helper->current_branch->tracked_files[i].name); 
                container[container_size - 1].change_type = 2;
                
                // Adding to current commits info:
                // This is the commit we need to add to -> temp_helper->current_branch->commits[temp_helper->current_branch->n_commits-1]
                
                tmp_cmt->n_changed_files++;
                int temp_n = tmp_cmt->n_changed_files;
                
                tmp_cmt->changed_files = realloc(
                tmp_cmt->changed_files,temp_n*sizeof(struct change_file));
                tmp_cmt->changed_files[temp_n - 1].name = malloc(strlen(temp_helper->current_branch->tracked_files[i].name) + 1);
                tmp_cmt->changed_files[temp_n - 1].name = strcpy(tmp_cmt->changed_files[temp_n - 1].name,temp_helper->current_branch->tracked_files[i].name);
                tmp_cmt->changed_files[temp_n - 1].previous_hash = old_hash;
                tmp_cmt->changed_files[temp_n - 1].new_hash = temp_helper->current_branch->tracked_files[i].hash;
                
            }
        }
        qsort(container, container_size, sizeof(struct commit_id_container), alphabetise_b);
        
        /*
            Now that we have an array of commit_id_containers we can sort through
            the array alphabetically and determine what change type each file is
        */
        for(int i = 0; i < container_size; i++){
            // Deletion
            if(container[i].change_type == 0){
                id += 85973;
                for(int j = 0; j < strlen(container[i].name); j++){
			        id = ((id*(container[i].name[j] % 37))%15485863) + 1;
		        }
            }
            // Addition
            if(container[i].change_type == 1){
                id += 376591;
                for(int j = 0; j < strlen(container[i].name); j++){
			        id = ((id*(container[i].name[j] % 37))%15485863) + 1;
		        }
            }
            // Modification
            if(container[i].change_type == 2){
                id += 9573681;
                for(int j = 0; j < strlen(container[i].name); j++){
			        id = ((id*(container[i].name[j] % 37))%15485863) + 1;
		        }
            }
        }
        if(container_size > 0){
            for(int i = 0; i < container_size; i++){
                free(container[i].name);
            }
        }
        free(container);
        sprintf(temp_hex, "%x", id);
        
        // String pad if necessary to 6 characters
        if(strlen(temp_hex) < 6){
            int space = 6 - strlen(temp_hex);
            char* new_hex = malloc(sizeof(char)*7);
            sprintf(new_hex,"%0*d%s",space,0,temp_hex);
            temp_hex = strcpy(temp_hex,new_hex);
            free(new_hex);
        }
    }
}

char *create_commit(void *helper, char *message){
    helper_t* temp_helper = (helper_t*)helper;
    // Update no. of commits and resize
                
    temp_helper->current_branch->n_commits++;
    int n_commits = temp_helper->current_branch->n_commits;
    temp_helper->current_branch->head_index = temp_helper->current_branch->n_commits - 1;
                
    temp_helper->current_branch->commits = realloc(temp_helper->current_branch->commits,sizeof(struct commit)*n_commits);
    
    // Use a temp pointer to reduce code clutter
    struct commit* temp_cmt_ptr =  &temp_helper->current_branch->commits[n_commits - 1];
                
    // Copy string message
    temp_cmt_ptr->message = malloc(strlen(message) + 1);
    temp_cmt_ptr->message = strcpy(temp_cmt_ptr->message,message);

    // Copy over number of files
    temp_cmt_ptr->n_files = temp_helper->current_branch->n_tracked_files;
    
    temp_cmt_ptr->n_added_files = 0;
    temp_cmt_ptr->n_removed_files = 0;
    temp_cmt_ptr->n_changed_files = 0;
    
    temp_cmt_ptr->added_files = malloc(sizeof(char*));
    temp_cmt_ptr->removed_files = malloc(sizeof(char*));
    temp_cmt_ptr->changed_files = malloc(sizeof(struct change_file));
                
    /*
        Deep copy into the commits directory. First we need to calculate the commit id, since this
        is the first commit there will be no changes so calculating the ID is fairly simplistic.
    */

    char temp_hex[10] = "XXXXXXXXXX";
    get_commit_id(helper, message, temp_hex);
    
    // Assign the ID to the commit.
    temp_cmt_ptr->id = malloc(strlen(temp_hex) + 1);
    temp_cmt_ptr->id = strcpy(temp_cmt_ptr->id,temp_hex);
    char sub_directory[strlen(temp_helper->current_branch->directory) + strlen(temp_hex) + 2];
    
    strcpy(sub_directory,temp_helper->current_branch->directory);
    strcat(sub_directory,"/");
    strcat(sub_directory,temp_hex);
    // Create directory for commit
    int check;
    check = mkdir(sub_directory,0777);
    if(check){
        printf("Directory already exists!\n");
    }
    // Set location in commit file
    temp_cmt_ptr->location = malloc(strlen(sub_directory)+1);
    temp_cmt_ptr->location = strcpy(temp_cmt_ptr->location,sub_directory);
    // Copy files into commit directory
    copy_files(sub_directory,temp_helper->current_branch);
    // Copy over file_obj struct
    temp_cmt_ptr->files = malloc(sizeof(struct file_obj)*temp_helper->current_branch->n_tracked_files);
    for(int k = 0; k < temp_helper->current_branch->n_tracked_files; k++){
        temp_cmt_ptr->files[k].name=malloc(strlen(temp_helper->current_branch->tracked_files[k].name) + 1);
        temp_cmt_ptr->files[k].path=malloc(strlen(temp_helper->current_branch->tracked_files[k].path) + 1);
        
        temp_cmt_ptr->files[k].name=strcpy(temp_cmt_ptr->files[k].name,temp_helper->current_branch->tracked_files[k].name);
        temp_cmt_ptr->files[k].path=strcpy(temp_cmt_ptr->files[k].path,temp_helper->current_branch->tracked_files[k].path);
        temp_cmt_ptr->files[k].hash=temp_helper->current_branch->tracked_files[k].hash;
    }
    return temp_helper->current_branch->commits[n_commits - 1].id;
}

char *svc_commit(void *helper, char *message) {
    if(message == NULL){
        return NULL;
    }
    helper_t* temp_helper = (helper_t*)helper;
    
    /*
        Check for manual deletions of files,
        and manual editing of files before commit is proceeded with
    */
    check_deletion(helper);
    rehash_files(helper);
    
    if(temp_helper->current_branch->n_tracked_files == 0){
        return NULL;
    }
    
    if(temp_helper->current_branch->n_commits == 0){
        temp_helper->current_branch->n_commits++;
        temp_helper->current_branch->head_index = temp_helper->current_branch->n_commits - 1;
        temp_helper->current_branch->commits = malloc(sizeof(struct commit));
        
        struct commit* tmp_cmt_ptr = &temp_helper->current_branch->commits[0];
        
        // Copy string message
        tmp_cmt_ptr->message = malloc(strlen(message) + 1);
        tmp_cmt_ptr->message = strcpy( tmp_cmt_ptr->message,message);
        tmp_cmt_ptr->n_files = temp_helper->current_branch->n_tracked_files;
        
        tmp_cmt_ptr->n_added_files = 0;
        tmp_cmt_ptr->n_removed_files = 0;
        tmp_cmt_ptr->n_changed_files = 0;
        
        tmp_cmt_ptr->added_files = malloc(sizeof(char*));
        tmp_cmt_ptr->removed_files = malloc(sizeof(char*));
        tmp_cmt_ptr->changed_files = malloc(sizeof(struct change_file));
        
        // Create directory and assign to the branch
        int check;
        char directory[strlen(temp_helper->current_branch->name) + 9];
        strcpy(directory, "commits/");
        strcat(directory,temp_helper->current_branch->name);
        check = mkdir(directory,0777);
        if(check){
            printf("Directory already exists!\n");
        }
        // Copy over directory into the current branch
        temp_helper->current_branch->directory = malloc(strlen(directory) + 1);
        temp_helper->current_branch->directory = strcpy(temp_helper->current_branch->directory,directory);
        /*
            Deep copy into the commits directory
            First we need to calculate the commit id, since this is the first commit 
            there will be no changes so calculating the ID is fairly simplistic.
        */
        char temp_hex[10] = "XXXXXXXXXX";
        
        get_commit_id(helper, message, temp_hex);

        // Assign the ID to the commit.
        tmp_cmt_ptr->id = malloc(strlen(temp_hex) + 1);
        tmp_cmt_ptr->id = strcpy(tmp_cmt_ptr->id,temp_hex);
        
        char sub_directory[strlen(directory) + strlen(temp_hex) + 2];
        strcpy(sub_directory,directory);
        strcat(sub_directory,"/");
        strcat(sub_directory,temp_hex);
        
        // Create directory for commit
        check = mkdir(sub_directory,0777);
        if(check){
            printf("Directory already exists!\n");
        }
        // Set location in commit file
        tmp_cmt_ptr->location = malloc(strlen(sub_directory) + 1);
        tmp_cmt_ptr->location = strcpy(tmp_cmt_ptr->location,sub_directory);
        
        // Copy files into commit directory
        copy_files(sub_directory,temp_helper->current_branch);
        // Copy over file_obj struct
        tmp_cmt_ptr->files = malloc(sizeof(struct file_obj)*temp_helper->current_branch->n_tracked_files);
        
        for(int k = 0; k < temp_helper->current_branch->n_tracked_files; k++){
            tmp_cmt_ptr->files[k].name=malloc(strlen(temp_helper->current_branch->tracked_files[k].name) + 1);
            tmp_cmt_ptr->files[k].path=malloc(strlen(temp_helper->current_branch->tracked_files[k].path) + 1);
            
            tmp_cmt_ptr->files[k].name=strcpy(tmp_cmt_ptr->files[k].name,temp_helper->current_branch->tracked_files[k].name);
            tmp_cmt_ptr->files[k].path=strcpy(tmp_cmt_ptr->files[k].path,temp_helper->current_branch->tracked_files[k].path);
            tmp_cmt_ptr->files[k].hash=temp_helper->current_branch->tracked_files[k].hash;
        }
        temp_helper->current_branch->up_to_date = 1;
        return tmp_cmt_ptr->id;
        
    // We have more than one commit. This is NOT the first commit
    }else if(temp_helper->current_branch->n_commits > 0){      
        /*
            Check for changes by doing simple comparisons.
            Compare file hashes, and check if n files is the same
        */
        struct commit* current_commit = &temp_helper->current_branch->commits[temp_helper->current_branch->n_commits-1];
        if(temp_helper->current_branch->n_tracked_files == current_commit->n_files){
            // Next, order by HASH int, and compare each hash int. If they aren't equal, then a change has been made.
            // If they are all equal, no change made.
            
            qsort(temp_helper->current_branch->tracked_files, temp_helper->current_branch->n_tracked_files,sizeof(struct file_obj), int_sort);
            qsort(current_commit->files,current_commit->n_files,sizeof(struct file_obj), int_sort);
            
            int change_flag = 0;
            for(int i = 0; i < temp_helper->current_branch->n_tracked_files; i++){
                if(temp_helper->current_branch->tracked_files[i].hash != current_commit->files[i].hash){
                    change_flag = 1;     
                }
            }
            if (change_flag == 0){
                // No change since last commit
                return NULL;
            }else{
                // Change since last commit
                temp_helper->current_branch->up_to_date = 1;
                return(create_commit(helper,message));
            }
        // Different number of files so we need to make a new commit for sure!
        }else{
            temp_helper->current_branch->up_to_date = 1;
            return(create_commit(helper,message));
        }
    }else{
        return NULL;
    }
    return NULL;
}

void *get_commit(void *helper, char *commit_id) {
    // We will need to check every branch for the commit ID.
    helper_t* temp_helper = (helper_t*)helper;
    if(commit_id == NULL){
        return NULL;
    }
    for(int i = 0; i < temp_helper->n_branches; i++){
        // Cycle through the branches
        for(int j = 0; j < temp_helper->branches[i].n_commits; j++){
            // Cycle through the commits
            if(!strcmp(temp_helper->branches[i].commits[j].id,commit_id)){
                return &(temp_helper->branches[i].commits[j]);
            }
        }
    }
    return NULL;
}

void *get_commit_branch(void *helper, char *commit_id) {
    // We will need to check every branch for the commit ID.
    helper_t* temp_helper = (helper_t*)helper;
    if(commit_id == NULL){
        return NULL;
    }
    for(int i = 0; i < temp_helper->n_branches; i++){
        // Cycle through the branches
        for(int j = 0; j < temp_helper->branches[i].n_commits; j++){
            // Cycle through the commits
            if(!strcmp(temp_helper->branches[i].commits[j].id,commit_id)){
                return &(temp_helper->branches[i]);
            }
        }
    }
    return NULL;
}

int get_commit_index(void *helper, char *commit_id){
    // We will need to check every branch for the commit ID.
    helper_t* temp_helper = (helper_t*)helper;
    if(commit_id == NULL){
        return -1;
    }
    for(int i = 0; i < temp_helper->n_branches; i++){
        // Cycle through the branches
        for(int j = 0; j < temp_helper->branches[i].n_commits; j++){
            // Cycle through the commits
            if(!strcmp(temp_helper->branches[i].commits[j].id,commit_id)){
                return j;
            }
        }
    }
    return -1;
}

char **get_prev_commits(void *helper, void *commit, int *n_prev) {
    if(n_prev == NULL){
        return NULL;
    }
    if(commit == NULL){
        *n_prev = 0;
        return NULL;
    }
    struct commit *commit_ptr = (struct commit*)commit; // We should typecast commit so that we can use it more easily.
    int commit_index = get_commit_index(helper,commit_ptr->id); // Equivalent to the the n_prev commits
    
    // In case of being the first commit
    if(commit_index == 0){
        *n_prev = 0;
        return NULL;
    }else{
        *n_prev = commit_index;
    }
    struct branch *branch_ptr = get_commit_branch(helper,commit_ptr->id);
    
    char** strings;
    strings = malloc((commit_index) * sizeof(char*));
    for(int i = commit_index - 1; i >= 0; i--){
         // Now we need to populate our string ptr array.
        strings[i] = branch_ptr->commits[i].id;
    }
    return strings;
}

void print_commit(void *helper, char *commit_id) {
    if(commit_id == NULL){
        printf("Invalid commit id\n");
        return;
    }
    helper_t* temp_helper = (helper_t*)helper;
    struct commit* cmt_ptr = get_commit(temp_helper,commit_id);
    if(cmt_ptr == NULL){
        printf("Invalid commit id\n");
        return;
    }
    
    printf("%s [%s]: %s\n",cmt_ptr->id,((struct branch*)get_commit_branch(helper,commit_id))->name,cmt_ptr->message);
    for(int i = 0; i < cmt_ptr->n_added_files; i++){
        printf("    + %s\n",cmt_ptr->added_files[i]);
    }
    for(int i = 0; i < cmt_ptr->n_removed_files; i++){
        printf("    - %s\n",cmt_ptr->removed_files[i]);
    }
    // MODIFICATION IDS MUST BE PADDED to 10 characters wide
    for(int i = 0; i < cmt_ptr->n_changed_files; i++){
        printf("    / %s [%10d-->%10d]\n",cmt_ptr->changed_files[i].name,
               cmt_ptr->changed_files[i].previous_hash,cmt_ptr->changed_files[i].new_hash);
    }
    printf("\n");
    printf("    Tracked files (%d):\n",cmt_ptr->n_files);
    for(int i = 0; i < cmt_ptr->n_files; i++){
        printf("    [%10d] ",cmt_ptr->files[i].hash);
        printf("%s\n",cmt_ptr->files[i].name);
        
    }
}

int sanitize_name(char* branch_name){
    /* Comparison to int values table:
        a-z: 49-74
        A-Z: 17-42
        0-9: 0-9
        _: 47
        /: -1
        -: -3
    */
    int library[66] = {49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,
                      71,72,73,74,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,
                      35,36,37,38,39,40,41,42,0,1,2,3,4,5,6,7,8,9,0,47,1,3};
    
    for(int i = 0; i < strlen(branch_name); i++){
        int c = branch_name[i] - '0';
        // Since 1, 3, -1, and -3 would be appropriate this can be done.
        if(c == -1){
            c = 1;
        }
        if(c == -3){
            c = 3;
        }
        int fail_flag = 0;
        for(int j = 0; j < 66; j++){
            if(c == library[j]){
                fail_flag = 1;
            }
        }
        if(fail_flag == 0){
            return 1;
        }
    }
    return 0;
}

int svc_branch(void *helper, char *branch_name) {
    helper_t* temp_helper = (helper_t*)helper;
    if(branch_name == NULL){
        return -1;
    }
    if(sanitize_name(branch_name) == 0){
        // The name is valid
    }else{
        return -1;
    }
    
    if(temp_helper->current_branch->up_to_date == 0){
        return -3;
    }
    
    for(int i = 0; i < temp_helper->n_branches; i++){
        if(!strcmp(temp_helper->branches[i].name,branch_name)){
            return -2;
        }
    }
    temp_helper->n_branches++;
    temp_helper->branches = realloc(temp_helper->branches,sizeof(struct branch)*temp_helper->n_branches);
    struct branch* temp_ptr = &(temp_helper->branches[temp_helper->n_branches-1]);// Create temporary branch ptr to reduce code bulk
    temp_ptr->name = malloc(strlen(branch_name)+1);
    temp_ptr->name = strcpy(temp_helper->branches[temp_helper->n_branches-1].name,branch_name);
    temp_helper->current_branch = &(temp_helper->branches[temp_helper->curr_branch_index]);
    /*
        We will need to perform a deep copy of the branch being branched from
    */
    
    // Update tracked files
    temp_ptr->n_commits = temp_helper->current_branch->n_commits;
    temp_ptr->n_tracked_files = temp_helper->current_branch->n_tracked_files;
    
    // Copy over tracked files now
    temp_ptr->tracked_files = malloc(sizeof(struct file_obj)*temp_helper->current_branch->n_tracked_files);
    for(int i = 0; i < temp_helper->current_branch->n_tracked_files; i++){
        temp_ptr->tracked_files[i].name = malloc(strlen(temp_helper->current_branch->tracked_files[i].name)+1);
        temp_ptr->tracked_files[i].path = malloc(strlen(temp_helper->current_branch->tracked_files[i].path)+1);
        
        temp_ptr->tracked_files[i].name = strcpy(temp_ptr->tracked_files[i].name,temp_helper->current_branch->tracked_files[i].name);
        temp_ptr->tracked_files[i].path = strcpy(temp_ptr->tracked_files[i].path,temp_helper->current_branch->tracked_files[i].path);
        temp_ptr->tracked_files[i].hash = temp_helper->current_branch->tracked_files[i].hash;
    }
    
    // Now we copy the commits.
    temp_ptr->head_index = temp_helper->current_branch->head_index;
    temp_ptr->commits = malloc(sizeof(struct commit)*temp_helper->current_branch->n_commits);
    
    for(int i = 0; i < temp_helper->current_branch->n_commits; i++){
        temp_ptr->commits[i].id = malloc(strlen(temp_helper->current_branch->commits[i].id)+1);
        temp_ptr->commits[i].message = malloc(strlen(temp_helper->current_branch->commits[i].message)+1);
        temp_ptr->commits[i].location = malloc(strlen(temp_helper->current_branch->commits[i].location)+1);
        
        temp_ptr->commits[i].id = strcpy(temp_ptr->commits[i].id,temp_helper->current_branch->commits[i].id);
        temp_ptr->commits[i].message = strcpy(temp_ptr->commits[i].message,temp_helper->current_branch->commits[i].message); 
        temp_ptr->commits[i].location = strcpy(temp_ptr->commits[i].location,temp_helper->current_branch->commits[i].location);
        
        temp_ptr->commits[i].n_files = temp_helper->current_branch->commits[i].n_files;
        temp_ptr->commits[i].n_added_files = temp_helper->current_branch->commits[i].n_added_files;
        temp_ptr->commits[i].n_removed_files = temp_helper->current_branch->commits[i].n_removed_files;
        temp_ptr->commits[i].n_changed_files = temp_helper->current_branch->commits[i].n_changed_files;
        
        // Copy over file_obj struct array
        temp_ptr->commits[i].files = malloc(sizeof(struct file_obj)*temp_helper->current_branch->commits[i].n_files);
        
        for(int j = 0; j < temp_helper->current_branch->commits[i].n_files; j++){
            temp_ptr->commits[i].files[j].name = malloc(strlen(temp_helper->current_branch->commits[i].files[j].name)+1);
            temp_ptr->commits[i].files[j].path = malloc(strlen(temp_helper->current_branch->commits[i].files[j].path)+1);
            
            temp_ptr->commits[i].files[j].name = strcpy(temp_ptr->commits[i].files[j].name,temp_helper->current_branch->commits[i].files[j].name);
            temp_ptr->commits[i].files[j].path = strcpy(temp_ptr->commits[i].files[j].path,temp_helper->current_branch->commits[i].files[j].path);
            temp_ptr->commits[i].files[j].hash = temp_helper->current_branch->commits[i].files[j].hash;
        }
        temp_ptr->commits[i].added_files = malloc(sizeof(char*)*temp_helper->current_branch->commits[i].n_added_files);
        // Copy added_files array
        for(int j = 0; j < temp_helper->current_branch->commits[i].n_added_files; j++){
            temp_ptr->commits[i].added_files[j] = malloc(strlen(temp_helper->current_branch->commits[i].added_files[j])+1);
            temp_ptr->commits[i].added_files[j] = strcpy(temp_ptr->commits[i].added_files[j],temp_helper->current_branch->commits[i].added_files[j]);
        }
        temp_ptr->commits[i].removed_files = malloc(sizeof(char*)*temp_helper->current_branch->commits[i].n_removed_files);
        // Copy removed_files array
        for(int j = 0; j < temp_helper->current_branch->commits[i].n_removed_files; j++){
            temp_ptr->commits[i].removed_files[j] = malloc(strlen(temp_helper->current_branch->commits[i].removed_files[j])+1);
            temp_ptr->commits[i].removed_files[j] = strcpy(temp_ptr->commits[i].removed_files[j],temp_helper->current_branch->commits[i].removed_files[j]);
        }
       
        temp_ptr->commits[i].changed_files = malloc(sizeof(struct change_file)*temp_helper->current_branch->commits[i].n_changed_files);
        // Copy changed_files struct array
        for(int j = 0; j < temp_helper->current_branch->commits[i].n_changed_files; j++){
            temp_ptr->commits[i].changed_files[j].previous_hash = temp_helper->current_branch->commits[i].changed_files[j].previous_hash;
            temp_ptr->commits[i].changed_files[j].new_hash = temp_helper->current_branch->commits[i].changed_files[j].new_hash;
            temp_ptr->commits[i].changed_files[j].name = malloc(strlen(temp_helper->current_branch->commits[i].changed_files[j].name)+1);
            temp_ptr->commits[i].changed_files[j].name = strcpy(temp_ptr->commits[i].changed_files[j].name,temp_helper->current_branch->commits[i].changed_files[j].name);
        }
    }
    // Create the branches physical directory
    int check;
    char directory[strlen(temp_ptr->name) + 9];
    strcpy(directory, "commits/");
    strcat(directory,temp_ptr->name);
    check = mkdir(directory,0777);
    if(check){
        printf("Directory already exists!\n");
    }
    // Copy over directory into the current branch
    temp_ptr->directory = malloc(strlen(directory) + 1);
    temp_ptr->directory = strcpy(temp_ptr->directory,directory);
    temp_ptr->up_to_date = 1;
    return 0;
}

int svc_checkout(void *helper, char *branch_name) {
    helper_t* temp_helper = (helper_t*)helper;
    if(branch_name == NULL){
        return -1;
    }
    if(temp_helper->current_branch->up_to_date == 0){
        return -2;
    }
    int fail_flag = 1;
    int index = 0;
    /* 
        Loop through branches to find the index of the correct branch, if the branch doesn't exist,
        set the fail_flag to 1. Otherwise, update the index variable with the index of the branch.
    */
    for(int i = 0; i < temp_helper->n_branches; i++){
        if(!strcmp(temp_helper->branches[i].name,branch_name)){
            // Branch found.
            fail_flag = 0;
            index = i;
        }
    }
    if(fail_flag == 1){
        return -1;
    }
    temp_helper->curr_branch_index = index;
    temp_helper->current_branch = &(temp_helper->branches[index]);
    return 0;
}

char **list_branches(void *helper, int *n_branches) {
    helper_t* temp_helper = (helper_t*)helper;
    if(n_branches == NULL){
        return NULL;
    }
    *n_branches = temp_helper->n_branches;
    char** strings;
    strings = malloc(temp_helper->n_branches * sizeof(char*));
    for(int i = 0; i < temp_helper->n_branches; i++){
        printf("%s\n",temp_helper->branches[i].name);
        strings[i] = (temp_helper->branches[i].name);
    }
    return strings;
}

int svc_add(void *helper, char *file_name) {
    if(file_name == NULL){
        return -1;
    }
    FILE* file = fopen(file_name, "r");
    if(file == NULL){
        return -3;
    }
    helper_t* temp_helper = (helper_t*)helper;
    
    int temp_hash = hash_file(helper,file_name);
    int fail_flag = 0;
    for(int i = 0; i < temp_helper->current_branch->n_tracked_files; i++){
        if(temp_hash == temp_helper->current_branch->tracked_files[i].hash){
            fail_flag = 1;
        }
    }
    if(fail_flag == 1){
        return -2;
    }else{
        temp_helper->current_branch->up_to_date = 0;
        temp_helper->current_branch->n_tracked_files++;
        int temp_n = temp_helper->current_branch->n_tracked_files;
        if(temp_helper->current_branch->n_tracked_files == 1){
            temp_helper->current_branch->tracked_files = malloc(sizeof(struct file_obj));
        }else{
            temp_helper->current_branch->tracked_files = realloc(temp_helper->current_branch->tracked_files,sizeof(struct file_obj)*temp_n);
        }
        
        struct file_obj* tmp_file_ptr = &temp_helper->current_branch->tracked_files[temp_n-1];
        tmp_file_ptr->path = malloc(strlen(file_name) + 1);
        tmp_file_ptr->name = malloc(strlen(file_name) + 1);
        
        tmp_file_ptr->path = strcpy(tmp_file_ptr->path,file_name);
        tmp_file_ptr->name = strcpy(tmp_file_ptr->name,file_name);
        tmp_file_ptr->hash = temp_hash;
        return temp_hash;
    }
}

void rm_tracked_files(void* helper, char *file_name){
    helper_t* temp_helper = (helper_t*)helper;
    // Now we need to remove the file from tracked files
    temp_helper->current_branch->n_tracked_files--;
    if(temp_helper->current_branch->n_tracked_files != 0){
        struct file_obj* tracked_files_temp = malloc(sizeof(struct file_obj)*temp_helper->current_branch->n_tracked_files);
        int curr_pos = 0;
        // Populate temporary array with new data.
        for(int j = 0; j < temp_helper->current_branch->n_tracked_files + 1; j++){
            if(!(strcmp(temp_helper->current_branch->tracked_files[j].name,file_name) == 0)){
                tracked_files_temp[curr_pos].name = malloc(strlen(temp_helper->current_branch->tracked_files[j].name) + 1);
                tracked_files_temp[curr_pos].path = malloc(strlen(temp_helper->current_branch->tracked_files[j].path) + 1);
                
                tracked_files_temp[curr_pos].name = strcpy(tracked_files_temp[curr_pos].name,temp_helper->current_branch->tracked_files[j].name);
                tracked_files_temp[curr_pos].path = strcpy(tracked_files_temp[curr_pos].path,temp_helper->current_branch->tracked_files[j].path);
                tracked_files_temp[curr_pos].hash = temp_helper->current_branch->tracked_files[j].hash;
                
                curr_pos++;
            }
        }
        // Free old array
        for(int k = 0; k < temp_helper->current_branch->n_tracked_files + 1; k++){
            free(temp_helper->current_branch->tracked_files[k].name);
            free(temp_helper->current_branch->tracked_files[k].path);
        }
        // Copy temporary array 
        free(temp_helper->current_branch->tracked_files);
        temp_helper->current_branch->tracked_files = tracked_files_temp;
    }else{
        for(int k = 0; k < temp_helper->current_branch->n_tracked_files + 1; k++){
            free(temp_helper->current_branch->tracked_files[k].name);
            free(temp_helper->current_branch->tracked_files[k].path);
            free(temp_helper->current_branch->tracked_files);
        }
    }
}

int svc_rm(void *helper, char *file_name) {
    if(file_name == NULL){
        return -1;
    }
    helper_t* temp_helper = (helper_t*)helper;
    int tracked_flag = 0;
    for(int i = 0; i < temp_helper->current_branch->n_tracked_files; i++){
        if(!strcmp(temp_helper->current_branch->tracked_files[i].name,file_name)){
            tracked_flag = 1;
        }
    }
    if(tracked_flag){
        temp_helper->current_branch->up_to_date = 0;
        if(temp_helper->current_branch->n_commits == 0){
            // Since no commits, return last known hash from add.
            int return_hash;
            for(int i = 0; i < temp_helper->current_branch->n_tracked_files; i++){
                if(!strcmp(temp_helper->current_branch->tracked_files[i].name,file_name)){
                    return_hash = temp_helper->current_branch->tracked_files[i].hash;
                }
            }
            rm_tracked_files(helper,file_name);
            return(return_hash);
        }else{
            // Since there have been commits, return last known hash from last commit.
            int return_hash;
            struct commit* tmp_head_ptr = &temp_helper->current_branch->commits[temp_helper->current_branch->head_index];
            for(int i = 0; i < tmp_head_ptr->n_files; i++){
                if(!strcmp(tmp_head_ptr->files[i].name,file_name)){
                    return_hash = tmp_head_ptr->files[i].hash;
                }
            }
            rm_tracked_files(helper,file_name);
            return(return_hash);
        }
    }else{
        return -2;
    }
    return 0;
}

void reset_file(char* dest, char* src){
    // Write the contents of file from dest -> src
    FILE * source;
    FILE * destination;
    source = fopen(src, "r");
    destination = fopen(dest, "w");
    char ch;
    while((ch = fgetc(source)) != EOF){
        fputc(ch, destination);
    }
    fclose(source);
    fclose(destination);
    return;
}

int svc_reset(void *helper, char *commit_id) {
    if(commit_id == NULL){
        return -1;
    }
    if(get_commit_index(helper,commit_id) == -1){
        return -2;
    }
    // Fetch the commit index and update all parts of the branch except for the n_commits and the directory
    helper_t* temp_helper = (helper_t*)helper;
    int index = get_commit_index(helper,commit_id);

    temp_helper->current_branch->head_index = index;
    temp_helper->current_branch->up_to_date = 1;
    
    if(get_commit(helper,commit_id) == NULL){
        return -2;
    }
    struct commit *commit_ptr = (struct commit*)get_commit(helper,commit_id);
    
    // Free all files then re-assign them from the resetted commit.
    for(int i = 0; i < temp_helper->current_branch->n_tracked_files; i++){
        free(temp_helper->current_branch->tracked_files[i].name);
        free(temp_helper->current_branch->tracked_files[i].path);
    }
    temp_helper->current_branch->n_tracked_files = commit_ptr->n_files;
    temp_helper->current_branch->tracked_files = realloc(temp_helper->current_branch->tracked_files,sizeof(struct file_obj)*commit_ptr->n_files);
    
    for(int i = 0; i < commit_ptr->n_files; i++){
        temp_helper->current_branch->tracked_files[i].name = malloc(strlen(commit_ptr->files[i].name) + 1);
        temp_helper->current_branch->tracked_files[i].path = malloc(strlen(commit_ptr->files[i].path) + 1);
        
        temp_helper->current_branch->tracked_files[i].name = strcpy(temp_helper->current_branch->tracked_files[i].name,commit_ptr->files[i].name);
        temp_helper->current_branch->tracked_files[i].path = strcpy(temp_helper->current_branch->tracked_files[i].path,commit_ptr->files[i].path);
        temp_helper->current_branch->tracked_files[i].hash = commit_ptr->files[i].hash;
    }
    
    // Resetting each file individually using the previous reset_file method implemented.
    for (int i = 0; i < temp_helper->current_branch->commits[temp_helper->current_branch->head_index].n_files; i++){
        // Produce filepath
        char str[100];
        sprintf(str, "%d", temp_helper->current_branch->commits[temp_helper->current_branch->head_index].files[i].hash);
        char path[strlen(commit_ptr->location) + strlen(str) + 2];
        strcpy(path,commit_ptr->location);
        strcat(path,"/");
        strcat(path,str);
        
        reset_file(temp_helper->current_branch->commits[temp_helper->current_branch->head_index].files[i].path,path);
    }
    return 0;
}

char *svc_merge(void *helper, char *branch_name, struct resolution *resolutions, int n_resolutions) {
    /*
      Unforunately I do not believe that this method is implemented properly.
      It produces somewhat the correct output but I am still trying to determine
      what in the method is bugged. As such, the code below is not correctly functioning.
    */
    if(branch_name == NULL){
        printf("Invalid branch name\n");
        return NULL;
    }
    helper_t* temp_helper = (helper_t*)helper;
    // Check if the branch exists.
    struct branch *branch_ptr = NULL;
    for(int i = 0; i < temp_helper->n_branches; i++){
        if(!strcmp(temp_helper->branches[i].name,branch_name)){
            if(!strcmp(temp_helper->current_branch->name,branch_name)){
                printf("Cannot merge a branch with itself\n");
                return NULL;
            }else{
                // Not the current checked out branch. Next step is to check for uncommited changes.
                branch_ptr = &(temp_helper->branches[i]);
            }
        }
    }
    // Branch doesn't exist
    if(branch_ptr == NULL){
        printf("Branch not found\n");
        return NULL;
    }
    /*
        Past this point it is determined that the branch does exist.
        Now we check for uncommited changes, and then proceed with the merge.
    */
    if(temp_helper->current_branch->up_to_date == 0){
        printf("Changes must be committed\n");
        return NULL;
    }
    
    // Merge procedure begins now
    for(int i = 0; i < n_resolutions; i++){
        if(resolutions[i].resolved_file == NULL){
            svc_rm(helper,resolutions[i].file_name);
        }else{
            svc_add(helper,resolutions[i].file_name);
        }
    }
    
    temp_helper->current_branch->n_commits++;
    temp_helper->current_branch->head_index++;
    temp_helper->current_branch->commits = realloc(temp_helper->current_branch->commits,sizeof(struct commit)*temp_helper->current_branch->n_commits);
    // Copy over ID
    struct commit* temp_cmt_ptr = &temp_helper->current_branch->commits[temp_helper->current_branch->n_commits-1];
    temp_cmt_ptr->id = malloc(strlen(branch_ptr->commits[branch_ptr->n_commits-1].id)+1);
    temp_cmt_ptr->message = malloc(strlen(branch_ptr->commits[branch_ptr->n_commits-1].message)+1);
    temp_cmt_ptr->location = malloc(strlen(branch_ptr->commits[branch_ptr->n_commits-1].location)+1);
    
    temp_cmt_ptr->id = strcpy(temp_cmt_ptr->id,branch_ptr->commits[branch_ptr->n_commits-1].id);
    temp_cmt_ptr->message = strcpy(temp_cmt_ptr->message,branch_ptr->commits[branch_ptr->n_commits-1].message);
    temp_cmt_ptr->location = strcpy(temp_cmt_ptr->location,branch_ptr->commits[branch_ptr->n_commits-1].location);
    temp_cmt_ptr->n_files = branch_ptr->commits[branch_ptr->n_commits-1].n_files;
    
    temp_cmt_ptr->files = malloc(sizeof(struct file_obj)*branch_ptr->commits[branch_ptr->n_commits-1].n_files);
    
    for(int i = 0; i < branch_ptr->commits[branch_ptr->n_commits-1].n_files; i++){
        temp_cmt_ptr->files[i].path = malloc(strlen(branch_ptr->commits[branch_ptr->n_commits-1].files[i].path)+1);
        temp_cmt_ptr->files[i].name = malloc(strlen(branch_ptr->commits[branch_ptr->n_commits-1].files[i].name)+1);
        
        temp_cmt_ptr->files[i].path = strcpy(temp_cmt_ptr->files[i].path,branch_ptr->commits[branch_ptr->n_commits-1].files[i].path);
        temp_cmt_ptr->files[i].name = strcpy(temp_cmt_ptr->files[i].name,branch_ptr->commits[branch_ptr->n_commits-1].files[i].name);
        temp_cmt_ptr->files[i].hash = branch_ptr->commits[branch_ptr->n_commits-1].files[i].hash;
    }
    
    temp_cmt_ptr->n_changed_files = branch_ptr->commits[branch_ptr->n_commits-1].n_changed_files;
    temp_cmt_ptr->n_added_files = branch_ptr->commits[branch_ptr->n_commits-1].n_added_files;
    temp_cmt_ptr->n_removed_files = branch_ptr->commits[branch_ptr->n_commits-1].n_removed_files;
    
    // Added files:
    temp_cmt_ptr->added_files = malloc(sizeof(char*)*branch_ptr->commits[branch_ptr->n_commits-1].n_added_files);
    
    for(int i = 0; i < branch_ptr->commits[branch_ptr->n_commits-1].n_added_files; i++){
        temp_cmt_ptr->added_files[i] = malloc(strlen(branch_ptr->commits[branch_ptr->n_commits-1].added_files[i])+1);
        temp_cmt_ptr->added_files[i] = strcpy(temp_cmt_ptr->added_files[i],branch_ptr->commits[branch_ptr->n_commits-1].added_files[i]);
    }
    // Removed files:
    temp_helper->current_branch->commits[temp_helper->current_branch->n_commits-1].removed_files = malloc(sizeof(char*)*branch_ptr->commits[branch_ptr->n_commits-1].n_removed_files);
    
    for(int i = 0; i < branch_ptr->commits[branch_ptr->n_commits-1].n_removed_files; i++){
        temp_cmt_ptr->removed_files[i] = malloc(strlen(branch_ptr->commits[branch_ptr->n_commits-1].removed_files[i])+1);
        temp_cmt_ptr->removed_files[i] = strcpy(temp_cmt_ptr->removed_files[i],branch_ptr->commits[branch_ptr->n_commits-1].removed_files[i]);
    }
    
    // Changed files:
    temp_cmt_ptr->changed_files = malloc(sizeof(struct change_file)*branch_ptr->commits[branch_ptr->n_commits-1].n_changed_files);
    
    for(int i = 0; i < branch_ptr->commits[branch_ptr->n_commits-1].n_changed_files; i++){
        temp_cmt_ptr->changed_files[i].name = malloc(strlen(branch_ptr->commits[branch_ptr->n_commits-1].changed_files[i].name)+1);
        temp_cmt_ptr->changed_files[i].name = strcpy(temp_cmt_ptr->changed_files[i].name,branch_ptr->commits[branch_ptr->n_commits-1].changed_files[i].name);
        
        temp_cmt_ptr->changed_files[i].new_hash = branch_ptr->commits[branch_ptr->n_commits-1].changed_files[i].new_hash;
        temp_cmt_ptr->changed_files[i].previous_hash = branch_ptr->commits[branch_ptr->n_commits-1].changed_files[i].previous_hash;
    }
    char message[30 + strlen(branch_name)];
    strcpy(message,"Merged branch ");
    strcat(message,branch_name);
    printf("Merge successful\n");
    
    return(svc_commit(helper,message));
}