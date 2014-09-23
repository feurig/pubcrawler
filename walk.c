/* dirStat.c
   This program computes directory statistics. The usage is:
      dirStat [-r] [<directory>],
   where -r enables recursive statistics and <directory> is the directory
   on which the statistics are computed (if not specified then cwd is used);
   The following statistics are computed:
     number of regular files/directories encountered and the amount of space 
   they use
     total space & number of files in the tree (without counting the same file
   twice if it has more than one link to it)
     total space linked outside the directory structure
*/   
     
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>

/* this is the typedef for the linked list */
/* we need a linked list in order to keep track of used inodes */

typedef struct node {
   off_t size;       
   nlink_t links;    /* no. of hard links -- we decrease it as we encounter 
                        other files with the same inode */
   ino_t inode;      /* inode */
   struct node *next;  /* pointer to next node in the linked list */
} iinfo; 
 
/* This function prints the appropriate error message depending on its
   argument; it then stops the program returning code 1 */
   
void error(int eno) {
  switch(eno) {
    case 1:
      printf("Incorrect parameters.\n");
      printf("Usage: dirStat [-r] [<directory>]\n");
      break;
    case 2: 
      printf("Incorrect number of parameters.\n");
      printf("Usage: dirStat [-r] [<directory>]\n");
      break;
    case 3:
      perror("Memory allocation failed");
      break;
  }
  exit(1);
}


/* Strips the slashes at the end of the string passed as an argument; */
/* we use this for "aesthetic" reasons; although /usr/bin///ls */
/* is correct, strictly speaking, /usr/bin/ls is much nicer */

void strip_trailing_slashes(char *path) {
  char *last = path + strlen(path) - 1;  
  while ( last >= path && *last == '/')
    *last-- = '\0';
}

/* this function returns true, or false, depending on whether "inode" is
   already in the linked list or not; if it is, we decrease the "links"
   field; after ending the directory traversal, "links" should be 1 for 
   every inode, if there are no "outside" links */
   
int inlist(ino_t inode, iinfo *In) {
  iinfo *tmp;
  tmp = In;
  while (tmp != NULL) {
    if ( tmp->inode == inode ) {
      (tmp->links)--;
      return(1);
    }
    tmp=tmp->next;
  }
  return 0;
}

/* this function adds a node with the information contained in "size",
"links" and "inode" to the linked list */

void addtolist(off_t size, nlink_t links, ino_t inode, iinfo **In) {
  iinfo *tmp;
  tmp=(iinfo *) malloc(sizeof(iinfo));
  tmp->size=size; 
  tmp->links=links;
  tmp->inode=inode;
  tmp->next=*In;
  *In=tmp;
}

/* this is the recursive traversal function - the most important part of the
   program; the arguments are the "path" (directory) we're currently 
   processing, whether we should do this "rec"ursively or not (-r parameter
   was given), total number of dirs, and of file links processed (these two
   are transmitted by reference because we need their value in the main 
   function, and we don't want to use any global variables), and finally
   a pointer to the inode list (to the first element in the linked list) */
     
int traverse(char *path, int rec, int *dirs, int *links, iinfo **In) {
  DIR *cdir;
  struct dirent *dirp;
  char *filepath;
  struct stat buf;
  int length, filelinks=0, subdirs=0;
  long int fspace=0, sbdspace=0;
  
  /* opening the directory */ 
  if ( (cdir = opendir(path)) == NULL ) {
    perror(path);
    return 1;
  }
  
  /* for every file in the directory */
  while ( (dirp = readdir(cdir) ) != NULL) {
    strip_trailing_slashes(path);
    
    /* the next block puts in the "filepath" variable the path to the
       currently processed file */
       
    length = strlen(path) + strlen(dirp->d_name) + 2;   
    if ( ( filepath=(char *) malloc(sizeof(char)*length) ) == NULL ) error(3);
    *filepath='\0';
    strcat( strcat( strcat(filepath, path), "/"), dirp->d_name);  
    
    if ( lstat(filepath, &buf) == -1) {
      perror(filepath);
      continue;
    }
    
    /* processing the file appropriately depending of whether it is a regular
       file or a directory; otherwise just ignore it */
       
    if (S_ISREG(buf.st_mode)) {
      filelinks++; 
      fspace+=buf.st_size;
      
      /* if the inode for the current file is not already in the linked list
         then add it */
      if (!inlist(buf.st_ino, *In)) 
        addtolist(buf.st_size, buf.st_nlink, buf.st_ino, In);
    } else 
        if (S_ISDIR(buf.st_mode) && strcmp(dirp->d_name, ".") && 
                    strcmp( dirp->d_name, "..") ) {
          subdirs++;
          sbdspace+=buf.st_size;
          /* if the file is a directory and recursive traversal is enabled
             just call the function recursively for this subdirectory */
          if (rec) traverse(filepath, 1, dirs, links, In); 
        }
      
    free(filepath);    
  }
  
  closedir(cdir);
  
  *links+=filelinks;
  (*dirs)++;
  
  /* print statistics for current directory */
  
  printf("Directory: %s\n", path);
  printf("  Total file links: %d\n", filelinks);
  printf("  Total file space: %ld\n", fspace);
  printf("  Total sub-directories: %d\n", subdirs);
  printf("  Total sub-directory file space: %ld\n", sbdspace);
  return 0;
}

int main(int argc, char *argv[]) {
  int rec, files=0, links=0, dirs=0, fouts=0;
  long int filespc=0, spcouts=0;
  iinfo *tmp, *In=NULL;
  char *path;
  
  /* depending on the arguments given to the program, call the traversal
     function with the appropriate path and recursively or not */
     
  switch(argc) {
    case 1:
      path="."; rec=0;
      break;
    case 2: 
      if ( !strcmp(argv[1], "-r") ) {
        path="."; rec=1;
      } else {
        path=argv[1]; rec=0;
      } 
      break;
    case 3:
      if ( strcmp(argv[1], "-r") ) error(1);
        else {
        path=argv[2]; rec=1;
      }
      break;
    default: error(2);
  }
  
  /* print the total statistics */
  
  if (!traverse(path, rec, &dirs, &links, &In)) {
    printf("Total directories encountered: %d\n", dirs);
    printf("Total file links: %d\n", links);
    
    /* this is the tricky part: the actual number of files (and the disk 
       space they use) is counted using the linked list of inodes; if one
       of the files has "links" > 1, it means there are actually some hard
       links to the file outside the directory tree we examined */
       
    tmp = In;
    while (tmp != NULL) {
      files++;
      filespc+=tmp->size;
      if (tmp->links > 1 ) {
        spcouts+=tmp->size;
        fouts++;
      }
      tmp = tmp->next;
    }
    printf("Total files: %d\n", files);
    printf("Total file space: %ld\n", filespc);
    printf("Files linked outside directory structure: %d\n", fouts);
    printf("File Space linked outside directory structure: %ld\n", spcouts);
  }
  
  return 0;
}
