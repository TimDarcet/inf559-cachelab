/* INF559 - cachelab
 * Project by Timothée Darcet timothee.darcet
 */

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

#include "cachelab.h"

#define WRITE_ALLOCATE 1 //unused for now, supposition is that we do write-allocate

/* Globals set by command line args */
int verbosity = 0; /* print trace if set */
int s = 0;         /* set index bits */
int b = 0;         /* block offset bits */
int E = 0;         /* associativity */
int t = 0;         /* tag length */
int S = 0;         /* number of sets */
int L = 0;         /* line size */
char *trace_file = NULL;
char *cache = NULL;
int *lastuse = NULL;
int timer = 0;

typedef struct line
{
    unsigned valid:1;
    unsigned int rest;
} line;

/*
 * printUsage - Print usage info
 */
void printUsage(char* argv[])
{
    printf("Usage: %s [-hv] -s <num> -E <num> -b <num> -t <file>\n", argv[0]);
    printf("Options:\n");
    printf("  -h         Print this help message.\n");
    printf("  -v         Optional verbose flag.\n");
    printf("  -s <num>   Number of set index bits.\n");
    printf("  -E <num>   Number of lines per set.\n");
    printf("  -b <num>   Number of block offset bits.\n");
    printf("  -t <file>  Trace file.\n");
    printf("\nExamples:\n");
    printf("  linux>  %s -s 4 -E 1 -b 4 -t traces/yi.trace\n", argv[0]);
    printf("  linux>  %s -v -s 8 -E 2 -b 4 -t traces/yi.trace\n", argv[0]);
    exit(0);
}

int load(unsigned int address) {
  // Try to find entry in cache
  unsigned int tag = address >> (32 - t);
  int sidx = (address << t) >> (32 - s);
  for (int lidx = 0; lidx < E; lidx++) {
    line *l = (struct line *) (cache + sidx * E * L + lidx * L);
    int currentTag = l->rest >> (32 - t);
    if (l->valid && (currentTag == tag)) {
      lastuse[sidx * E + lidx] = timer;
      timer++;
      if (verbosity)
        fprintf(stderr, "L %x hit\n", address);
      return 1; // hit
    }
  }
  // Entry not found
  // Try to replace an unused line
  for (int lidx = 0; lidx < E; lidx++) {
    line *l = (struct line *) (cache + sidx * E * L + lidx * L);
    if (!l->valid) {
      l->valid = 1;
      l->rest = tag << (32 - t);
      lastuse[sidx * E + lidx] = timer;
      timer++;
      if (verbosity)
        fprintf(stderr, "L %x miss\n", address);
      return 4; // miss
    }
  }
  // No unused line found
  // Find LRU line to evict
  int lru = 0;
  int lru_timestamp = timer;
  for (int lidx = 0; lidx < E; lidx++) {
    int ts = lastuse[sidx * E + lidx];
    if (ts < lru_timestamp) {
      lru_timestamp = ts;
      lru = lidx;
    }
  }
  line *l = (struct line *) (cache + sidx * E * L + lru * L);
  l->valid = 1;
  l->rest = tag << (32 - t);
  lastuse[sidx * E + lru] = timer;
  timer++;
  if (verbosity)
    fprintf(stderr, "L %x miss eviction\n", address);
  return 20; // miss, evict
}

int store(unsigned int address) {
  return load(address);
}

int modify(unsigned int address) {
  return load(address) + store(address);
}

void parse(int *counts) {
  FILE *fid;
  if ((fid = fopen(trace_file, "r")) != NULL) {
    while (!feof(fid)) {
      char operation;
      unsigned int address;
      int nBytes;
      if (fscanf(fid, " %c %x,%d\n", &operation, &address, &nBytes)) {
        if (verbosity)
          fprintf(stderr, " %c %x,%d\n", operation, address, nBytes);
        int ret;
        switch(operation) {
          case 'L':
            ret = load(address);
            break;
          case 'S':
            ret = store(address);
            break;
          case 'M':
            ret = modify(address);
            break;
          default:
            fprintf(stderr, "Invalid operation: %c\n", operation);
            ret = 0;
            break;
        }
        counts[0] += ret & 3;
        counts[1] += (ret >> 2) & 3;
        counts[2] += (ret >> 4) & 3;
        // printf("%d %d %d\n", counts[0], counts[1], counts[2]);
      }
    }
  }
  else {
    fprintf(stderr, "Couldn't open file\n");
  }
}

/*
 * main - Main routine 
 */
int main(int argc, char* argv[])
{
  char c;
  
  while( (c=getopt(argc,argv,"s:E:b:t:vh")) != -1){
    switch(c){
    case 's':
      s = atoi(optarg);
      break;
    case 'E':
      E = atoi(optarg);
      break;
    case 'b':
      b = atoi(optarg);
      break;
    case 't':
      trace_file = optarg;
      break;
    case 'v':
      verbosity = 1;
      break;
    case 'h':
      printUsage(argv);
      exit(0);
    default:
      printUsage(argv);
      exit(1);
    }
  }

  /* Make sure that all required command line args were specified */
  if (s == 0 || E == 0 || b == 0 || trace_file == NULL) {
    printf("%s: Missing required command line argument\n", argv[0]);
    printUsage(argv);
    exit(1);
  }

  t = 32 - b - s;
  S = (int)pow(2.0, (float)s);
  L = (1 + t + (int)pow(2, b));
  cache = calloc(S, E * L);
  lastuse = calloc(S * E, sizeof(int));
  int counts[3] = {0, 0, 0};
  parse(counts);
  free(cache);
  free(lastuse);
  printSummary(counts[0], counts[1], counts[2]);
  return 0;
}
