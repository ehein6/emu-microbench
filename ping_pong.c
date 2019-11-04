#include <stdlib.h>
#include <getopt.h>
#include <cilk.h>
#include <memoryweb.h>

replicated long num_migrations;
replicated long num_threads;
replicated long **results;

// ping pong function: starts at src, migrates 4 times to/from dst
void ping_pong(long *srcptr, long *dstptr)
{
  // Each iteration forces four migrations
  long n = num_migrations / 4;
  for (long i = 0; i < n; ++i) {
    MIGRATE(dstptr);
    MIGRATE(srcptr);
    MIGRATE(dstptr);
    MIGRATE(srcptr);
  }
}

// spawn threads for ping pong, compute elapsed time (cycles)
noinline long ping_pong_spawn(long *srcptr, long *dstptr)
{
  long t = num_threads;
  volatile unsigned long startnid = NODE_ID();
  volatile unsigned long starttime = CLOCK();
  for (long i = 0; i < t; ++i) cilk_spawn ping_pong(srcptr, dstptr);
  cilk_sync;
  volatile unsigned long endtime = CLOCK();
  volatile unsigned long endnid = NODE_ID();
  if (startnid != endnid) {
    printf("start NODE_ID %d end NODE_ID %d\n", startnid, endnid); exit(1); }
  long totaltime = endtime - starttime;
  return totaltime;
}

// one nodelet to nodelet ping_pong
void ping_pong_spawn_nlet(long src_nlet, long dst_nlet)
{
  if (src_nlet == dst_nlet) return;
  long *srcptr = mw_get_nth(&num_migrations, src_nlet); // get pointers
  long *dstptr = mw_get_nth(&num_migrations, dst_nlet);

  MIGRATE(srcptr); // migrate to source nodelet
  long cycles = ping_pong_spawn(srcptr, dstptr);
  results[src_nlet][dst_nlet] += cycles;
}

// iterates over all nlets for a given source or dest (<0 means loop)
void ping_pong_spawn_dist(long src_nlet, long dst_nlet)
{
  for (long nlet = 0; nlet < NODELETS(); ++nlet) {
    if (src_nlet < 0) ping_pong_spawn_nlet(nlet, dst_nlet);
    else if (dst_nlet < 0) ping_pong_spawn_nlet(src_nlet, nlet);
  } // only one is < 0 for this to be called
}

// all-to-all ping_pong, skip duplicates, params not used
void ping_pong_spawn_all(long src_nlet, long dst_nlet)
{
  for (long src_nlet = 0; src_nlet < NODELETS(); ++src_nlet) {
    for (long dst_nlet = 0; dst_nlet < NODELETS(); ++dst_nlet) {
      if (dst_nlet <= src_nlet) { continue; } // Skip duplicate trials
      ping_pong_spawn_nlet(src_nlet, dst_nlet);
    }
  }
}

// gather output; must be noinline or ping_pong doesn't work
noinline void gather(long ntr)
{
  printf("source dest cycles avg_time_ms million_mps latency_us\n");
  for (long i = 0; i < NODELETS(); ++i) {
    for (long j = 0; j < NODELETS(); ++j) {
      if (results[i][j] > 0) {
	long cycles = results[i][j];
	double time_ms = (double)cycles / (ntr * 175.0 * 1e3);
	double million_mps = (double)num_migrations / (time_ms * 1e3);
	double latency_us = (double)1e6 / million_mps;
	printf("%d %d %d %f %f %f\n", i, j,
	       cycles, time_ms, million_mps, latency_us);
      }
    }
  }
}

int main(int argc, char** argv)
{
  // default src<->dst, log2 migrations (4 per iteration), threads, trials
  long src = 1, dst = 2, log2_num = 3, nth = 2, ntr = 2;
  int c;
  while ((c = getopt(argc, argv, "hs:d:m:t:r:")) != -1) {
    switch (c) {
    case 'h':
      printf("Program options:\n");
      printf("\t-h print this help and exit\n");
      printf("\t-s <N> source nodelet (<0 = all) [%ld]\n", src);
      printf("\t-d <N> dest nodelet (<0 = all) [%ld]\n", dst);
      printf("\t-m <N> log2_num_migrations [%ld]\n", log2_num);
      printf("\t-t <N> number of threads [%ld]\n", nth);
      printf("\t-r <N> number of trials [%ld]\n", ntr);
      exit(0);
    case 's': src = atol(optarg); break;
    case 'd': dst = atol(optarg); break;
    case 'm': log2_num = atol(optarg); break;
    case 't': nth = atol(optarg); break;
    case 'r': ntr = atol(optarg); break;
    }
  }
  
  // check bounds on parameters
  if (src >= (long)NODELETS()) { printf("src_nlet too high\n"); exit(1); }
  if (dst >= (long)NODELETS()) { printf("dst_nlet too high\n"); exit(1); }
  if (log2_num <= 1) { printf("num_migrations must be >= 4\n"); exit(1); }
  if (nth <= 0) { printf("num_threads must be > 0\n"); exit(1); }
  if (ntr <= 0) { printf("num_trials must be > 0\n"); exit(1); }

  // log variables for the run
  long n = 1L << log2_num;
  printf("ping pong: num nodelets %d\n", NODELETS());
  printf("ping pong: src nlet %d\n", src);
  printf("ping pong: dst nlet %d\n", dst);
  printf("ping pong: num migrations %d\n", n);
  printf("ping pong: num threads %d\n", nth);
  printf("ping pong: num trials %d\n", ntr);
  fflush(stdout);

  // replicated variables so no migrations for loop bounds
  mw_replicated_init(&num_migrations, n);
  mw_replicated_init(&num_threads, nth);
  double **tmp_res = mw_malloc2d(NODELETS(), NODELETS() * sizeof(double));
  mw_replicated_init((long *)&results, (long)tmp_res);
  
  for (long i = 0; i < NODELETS(); ++i)
    for (long j = 0; j < NODELETS(); ++j)
      results[i][j] = 0; // initialize results to zero

#define RUN_BENCHMARK(X) for (long r = 0; r < ntr; ++r) X(src, dst)

  // 3 modes for benchmark depending on src/dst arguments
  MIGRATE(results[0]);
  starttiming();
  if ((src < 0) && (dst < 0)) RUN_BENCHMARK(ping_pong_spawn_all);
  else if ((src < 0) || (dst < 0)) RUN_BENCHMARK(ping_pong_spawn_dist);
  else RUN_BENCHMARK(ping_pong_spawn_nlet);
  MIGRATE(results[0]);

  // gather and print results
#ifndef DEBUG
  gather(ntr);
#endif
  return 0;
}
