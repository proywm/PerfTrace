#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <stdint.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/types.h>
#include <errno.h>

#include "splay-macros.h"
#include "fmt.h"

#define PSIZE 65535 


struct cct_node_t {
  uint32_t id;
  uint64_t ip;
  char name[128];
  struct cct_node_t *parent;
  struct cct_node_t *children;
  struct cct_node_t *left;
  struct cct_node_t *right;
  int tracenode;
};


static struct cct_node_t splay_root;
static uint32_t node_id = 0;
static uint64_t min_time = ~0;
static uint64_t max_time = 0;

char pname[PSIZE][128];
static int pname_it = 0;

void cct_init_node(struct cct_node_t *node)
{
  node->id = node_id;
  node_id += 1;
  node->ip = 0;
  node->parent = node->children = NULL;
  node->left = node->right = NULL;
  node->tracenode = 0;
}

#define l_lt(a, b) (strcmp(a, b)<0)
#define l_gt(a, b) (strcmp(a, b)>0)
struct cct_node_t*
splay(struct cct_node_t* cct, uint64_t ip, char *name)
{
//  REGULAR_SPLAY_TREE(cct_node_t, cct, ip, ip, left, right);
  GENERAL_SPLAY_TREE(cct_node_t, cct, name, name, name, left, right, l_lt, l_gt);
  return cct;
}

int
find_sym_name(char *name)
{
  int i;
  for (i = 0; i < pname_it; i++) {
    if(strcmp(pname[i], name) == 0)
      return i;
  }
  return 0;

}

/*************************************************************************/
struct cct_node_t *
insert_cct_ip(struct cct_node_t *node, uint64_t ip, char *name)
{
  if (!node) return NULL;

  struct cct_node_t *found = splay(node->children, ip, name);

//  if(found && found->ip == ip) return found;
  if(found && strcmp(found->name, name) == 0) return found;

  int i;
  for (i = 0; i < pname_it; i++) {
    if(strcmp(pname[i], name) == 0)
      break;
  }
  if (i == pname_it){
    strcpy(pname[pname_it], name);
    pname_it++;
    if (pname_it == PSIZE) {
      fprintf(stderr, "please enlarge your procedure map\n");
      exit(0);
    }
  }

  struct cct_node_t *new = (struct cct_node_t *)malloc(sizeof(struct cct_node_t));
  cct_init_node(new);
  strcpy(new->name, name);
  new->ip = ip;
  node->children = new;
  new->parent = node;
  if (!found)  return new;

//  if (ip < found->ip) {
  if (strcmp(name, found->name) < 0) {
    new->left = found->left;
    new->right = found;
    found->left = NULL;
  }
  else {
    new->left = found;
    new->right = found->right;
    found->right = NULL;
  }
  return new;
}

void cct_walk_node(struct cct_node_t *cct, void *arg);

void
walk_child_lrs(struct cct_node_t *cct, void *arg, void (*wf)(struct cct_node_t* n, void* a))
{
  if (!cct) return;
  walk_child_lrs(cct->left, arg, wf);
  walk_child_lrs(cct->right, arg, wf);
  wf(cct, arg);
}

void
cct_walk_node(struct cct_node_t *cct, void *arg)
{
  static int scope_id = 1;
  if (!cct ) return;
//printf("cct->name is %s\n", cct->name);
  FILE *xml_fd = (FILE *)arg;
  // not output the dummy root
  if(strcmp(cct->name, "root") != 0){
    int procedure_id = find_sym_name(cct->name) + 1;
    fprintf(xml_fd, "<PF i=\"%d\" s=\"%d\" l=\"0\" lm=\"0\" f=\"0\" n=\"%d\">\n", scope_id, procedure_id, procedure_id);
    scope_id++;
    // an internal cct node is logged as a trace node
    // need to explicitly declare it as a statement
    if (cct->tracenode == 1 && cct->children) {
      fprintf(xml_fd, "<S i=\"%d\" s=\"0\" l=\"0\" it=\"%u\">\n", scope_id, cct->id);
      scope_id++;
      fprintf(xml_fd, "</S>\n");
    
    }
    if(cct->children) {
      fprintf(xml_fd, "<C i=\"%d\" s=\"0\" l=\"0\">\n", scope_id);
      scope_id++;
    }
  }
  if(!cct->children)
  {
    fprintf(xml_fd, "<S i=\"%d\" s=\"0\" l=\"0\" it=\"%u\">\n", scope_id, cct->id);
    scope_id++;
    fprintf(xml_fd, "</S>\n");
  }
  walk_child_lrs(cct->children, arg, cct_walk_node);
  if(strcmp(cct->name, "root") != 0){
    if(cct->children)
      fprintf(xml_fd, "</C>\n");
    fprintf(xml_fd, "</PF>\n");
  }
}

void
write_out_cct(FILE *xml_fd)
{
  fprintf(xml_fd, "<HPCToolkitExperiment version=\"2.0\">\n");
  fprintf(xml_fd, "<Header n=\"system-wide\">\n");
  fprintf(xml_fd, "<Info/>\n");
  fprintf(xml_fd, "</Header>\n");
  fprintf(xml_fd, "<SecCallPathProfile i=\"0\" n=\"system-wide\">\n");
  fprintf(xml_fd, "<SecHeader>\n");
  fprintf(xml_fd, "<TraceDBTable>\n");
  fprintf(xml_fd, "<TraceDB i=\"0\" db-glob=\"*.hpctrace\" db-min-time=\"%"PRIu64"\" db-max-time=\"%"PRIu64"\" db-header-sz=\"24\"/>\n", min_time, max_time);
  fprintf(xml_fd, "</TraceDBTable>\n");
  fprintf(xml_fd, "<FileTable>\n");
  fprintf(xml_fd, "<File i=\"0\" n=\"dummy.c\"/>\n");
  fprintf(xml_fd, "</FileTable>\n");
  fprintf(xml_fd, "<ProcedureTable>\n");
  // to be inserted
//  fprintf(xml_fd, "<Procedure i=\"0\" n=\"IDLE\"/>\n");
  int i;
  for (i = 0; i < pname_it; i++) {
    fprintf(xml_fd, "<Procedure i=\"%d\" n=\"%s\"/>\n", i+1, pname[i]);
  }
  fprintf(xml_fd, "</ProcedureTable>\n");
  fprintf(xml_fd, "<Info/>\n");
  fprintf(xml_fd, "</SecHeader>\n");

  fprintf(xml_fd, "<SecCallPathProfileData>\n");

  // write out CCT
  struct cct_node_t *ptr = &splay_root;
  cct_walk_node(ptr, (void*)xml_fd);

  fprintf(xml_fd, "</SecCallPathProfileData>\n");
  fprintf(xml_fd, "</SecCallPathProfile>\n");
  fprintf(xml_fd, "</HPCToolkitExperiment>\n");
}


int read_file(FILE *fd, FILE *fd_new)
{
  uint64_t val64, size, time, pid, len;
  char name[1024];
  int ret;
  uint64_t stack[1024];
  char exename[1024];
  char stack_name[1024][128];

  // write header to the new file
  char magic[] = "HPCRUN-trace______";
  char version[] = "01.00";
  char endian[] = "b";

  fwrite(magic, sizeof(magic)-1, 1, fd_new);
  fwrite(version, sizeof(version)-1, 1, fd_new);
  fwrite(endian, sizeof(endian)-1 , 1, fd_new);

  while(1) {
    int stackit = 0;
    ret = fread(&len, sizeof(len), 1, fd);
    if (ret == 0) break;
    ret = fread(exename, len, 1, fd);
    if (ret == 0) break;
    exename[len] = '\0';

    ret = fread(&pid, sizeof(pid), 1, fd);
    if (ret == 0) break;

    // cpu
    ret = fread(&val64, sizeof(val64), 1, fd);
    if (ret == 0) break;

    ret = fread(&time, sizeof(time), 1, fd);
    if (ret == 0) break;
    // normalize to micro seconds
//    time = time/1000;
    if(time > max_time) max_time = time;
    if(time < min_time) min_time = time;

    while(1) {
      ret = fread(&val64, sizeof(val64), 1, fd);
      if (ret == 0 || val64 == 0x1111111) {
        break;
      }
      ret = fread(&len, sizeof(len), 1, fd);
      if (ret == 0) break;
      ret = fread(name, len, 1, fd);
      name[len] = '\0';
      // change all < and > to _, because hpctraceviewer cannot recognize them
      int namei = 0;
      for (namei = 0; namei < len; namei++) {
        if (name[namei] == '<' || name[namei] == '>')
          name[namei] = '_';
      }
      if (ret == 0) break;
      
      stack[stackit] = val64;
      strcpy(stack_name[stackit], name);
      stackit++;
      if (stackit == 1024) {
	fprintf(stderr, "too deep stack\n");
	exit(0);
      }
    }   
    struct cct_node_t *parent = &splay_root;
    parent->children = insert_cct_ip(parent, 0x1, exename);
    parent = parent->children;

    while(stackit--) {
       // splay tree: make sure the root is changed and need to be reset (use 5 hours to find this bug)
      parent->children = insert_cct_ip(parent, stack[stackit], stack_name[stackit]);
      parent = parent->children;
    }
    // not leaf
    if(parent->children) {
      parent->children = insert_cct_ip(parent, parent->ip, parent->name);
      parent = parent->children;
    }
    hpcio_be8_fwrite(&time, fd_new);
    hpcio_be4_fwrite(&(parent->id), fd_new);
    // this node is inserted into trace
    // make sure this node is the leaf node (no children)
    parent->tracenode = 1;
  }
  return 0;
}

int main(int argc, char **argv)
{
  char name[1024];
  if (argc <  2) {
    printf("please specify the directory that contains all sample files\n");
    exit(0);
  }
  cct_init_node(&splay_root);
  strcpy(splay_root.name, "root");

  DIR *dir = opendir(argv[1]);
  if (!dir) {
    printf("cannot open directory (permission or exitance)\n");
  }
  char dir_name[1024];
  sprintf(dir_name, "%s../hpctoolkit-syswide-database", argv[1]);
  mkdir(dir_name, 0755);

  struct dirent *ent;
  while((ent = readdir(dir)) != NULL) {
    if(strcmp(ent->d_name, ".") ==0 || strcmp(ent->d_name, "..")==0)
      continue;
//    printf("file with name %s\n", ent->d_name);
    sprintf(name, "%s/%s", argv[1], ent->d_name);
    FILE *fd = fopen(name, "rb");
    if (!fd) {
      printf("cannot open file (permission or exitance)\n");
      exit(0);
    }
    sprintf(name, "%s/%s.hpctrace",dir_name, ent->d_name);
    FILE *fd_new = fopen(name, "wb");
    if (!fd_new) {
      printf("cannot open file new(permission or exitance)\n");
      exit(0);
    }
    read_file(fd, fd_new);
  }
  closedir(dir);
  sprintf(name, "%s/experiment.xml", dir_name);
  FILE *xml_fd = fopen(name, "w");
  write_out_cct(xml_fd);
  fclose(xml_fd);
  return 0;

}
