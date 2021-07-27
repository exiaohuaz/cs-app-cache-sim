/* Name: Emily Zhang
   Andrew: exz

   csim.c is a cache simulator that takes in inputs s, E, and b
   and a trace file and outputs the number of resulting hits,
   misses, evictions, dirty_bytes, and dirty_evictions that result
   from running the trace.

   The cache is implemented using a struct that contains relevant
   input information and a set of Sets. A Set is a set of lines.
   Each line contains information about the validbit, tagbits,
   dirtybit, and timestamp for that line.
   The reason behind this implementation is that it very closely
   follows the structure of an actual cache -- we know that caches
   have 2^b sets, and each set has E lines, and each line has a
   valid bit, tag bit, and dirty bit.

   */

#include "cachelab.h"
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define STATNUM 5
#define ADDR_BITS 64

typedef struct Line {
    bool validbit;
    unsigned long tagbits;
    bool dirtybit;
    int timestamp;
} Line;

typedef struct Set {
    Line *lines;
} Set;

typedef struct Cache {
    int S, E, B, s, b;
    Set *sets;
} Cache;

/*  isInCache checks if an address is in the cache and updates cache
    to reflect the behavior of an actual cache. It modifies *result
    according to what counters should be incremented -- hit, miss,
    evictions, dirty_bytes, and/or dirty_evictions.

    Result is an bool array of length 5, where each index represents
    whether the corresponding counter should be incremented.
*/

void isInCache(unsigned long address, Cache *cache, char op, int num,
               bool *result) {

    unsigned long tagshift = cache->s + cache->b;
    unsigned long tag = address >> tagshift;
    unsigned long set = (address << (ADDR_BITS - cache->s - cache->b)) >>
                        (ADDR_BITS - cache->s);

    // When s is 0, above set calculation is undefined behavior.
    // set should be 0, so we set it to 0 manually.
    if (cache->s == 0) {
        set = 0;
    }

    bool notFull = false;

    int index = 0;

    int oldest = num;

    for (int i = 0; i < cache->E; i++) {

        // Detects a hit, exits early because address is found.
        if (cache->sets[set].lines[i].tagbits == tag &&
            cache->sets[set].lines[i].validbit == 1) {
            cache->sets[set].lines[i].timestamp = num;
            result[0] = true;
            if (op == 'S' && cache->sets[set].lines[i].dirtybit == false) {
                cache->sets[set].lines[i].dirtybit = true;
                result[3] = true;
            }
            return;
        }

        // Detects a cold miss, fills cache starting from lowest index i
        else if (cache->sets[set].lines[i].validbit == 0) {
            notFull = true;
            index = i;
            break;
        }

        // Otherwise, search for the least recently used line.
        // Timestamp is the instruction number. Smaller is older.
        if (cache->sets[set].lines[i].timestamp < oldest) {
            oldest = cache->sets[set].lines[i].timestamp;
            index = i;
        }
    }

    result[1] = true;

    cache->sets[set].lines[index].timestamp = num;
    cache->sets[set].lines[index].tagbits = tag;

    if (notFull) {
        cache->sets[set].lines[index].validbit = 1;
    } else {
        result[2] = true;
        if (cache->sets[set].lines[index].dirtybit) {
            cache->sets[set].lines[index].dirtybit = false;
            result[4] = true;
        }
    }

    if (op == 'S') {
        cache->sets[set].lines[index].dirtybit = true;
        result[3] = true;
    }

    return;
}

/*  makeCache fills the cache with initial information using the
    command line arguments s, E, and b, which are now stored in
    the cache.*/

void makeCache(Cache *cache) {
    cache->sets = malloc(sizeof(Set) * cache->S);
    for (int i = 0; i < cache->S; i++) {
        cache->sets[i].lines = malloc(sizeof(Line) * cache->E);
        for (int j = 0; j < cache->E; j++) {
            cache->sets[i].lines[j].validbit = false;
            cache->sets[i].lines[j].dirtybit = false;
            cache->sets[i].lines[j].timestamp = 0;
            cache->sets[i].lines[j].tagbits = 0;
        }
    }
}

/*  freeCache frees the cache by first freeing all lines by iterating
    through the sets, then freeing the sets, and lastly freeing the
    cache. This order is important because otherwise the pointer to
    the lines or sets would be lost before they are freed.*/

void freeCache(Cache *cache) {
    for (int i = 0; i < cache->S; i++) {
        free(cache->sets[i].lines);
    }
    free(cache->sets);
    free(cache);
}

/*  incrementStats reads instructions from the trace file and
    increments the stats according to the output of isInCache*/

void incrementStats(FILE *trace, csim_stats_t *stats, Cache *cache) {
    char instruction;
    unsigned long address;
    int size;
    char newline;
    bool *outcome = malloc(sizeof(bool) * 5);

    int linenum = 0;

    while (fscanf(trace, "%c %lx,%d%c", &instruction, &address, &size,
                  &newline) > 0) {

        // Resets outcome to false so iterations do not interfere with each
        // other
        for (int i = 0; i < STATNUM; i++) {
            outcome[i] = false;
        }

        isInCache(address, cache, instruction, linenum, outcome);

        if (outcome[0]) {
            stats->hits++;
        }
        if (outcome[1]) {
            stats->misses++;
        }
        if (outcome[2]) {
            stats->evictions++;
        }
        if (outcome[3]) {
            stats->dirty_bytes += cache->B;
        }
        if (outcome[4]) {
            stats->dirty_evictions += cache->B;
            stats->dirty_bytes -= cache->B;
        }
        linenum++;
    }

    free(outcome);
}

int main(int argc, char *argv[]) {

    Cache *cache = malloc(sizeof(Cache));

    // Uses command-line arguments to fill cache variables
    char *t = 0;
    int opt = 0;
    while ((opt = getopt(argc, argv, "s:E:b:t:")) != -1) {
        switch (opt) {
        case 's':
            cache->s = atoi(optarg);
            cache->S = 1 << (cache->s);
            break;
        case 'E':
            cache->E = atoi(optarg);
            break;
        case 'b':
            cache->b = atoi(optarg);
            cache->B = 1 << (cache->b);
            break;
        case 't':
            t = optarg;
            break;
        }
    }

    makeCache(cache);

    csim_stats_t *stats = malloc(sizeof(csim_stats_t));
    stats->hits = 0;
    stats->misses = 0;
    stats->evictions = 0;
    stats->dirty_bytes = 0;
    stats->dirty_evictions = 0;

    FILE *trace = fopen(t, "r");

    incrementStats(trace, stats, cache);

    printSummary(stats);

    free(stats);
    freeCache(cache);
    return 0;
}