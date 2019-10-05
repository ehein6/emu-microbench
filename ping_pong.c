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

noinline double ping_pong_spawn(long *srcptr, long *dstptr)
{
  long t = num_threads;
  long startnid = NODE_ID();
  unsigned long starttime = CLOCK();
  for (long i = 0; i < t; ++i) cilk_spawn ping_pong(srcptr, dstptr);
  cilk_sync;
  unsigned long endtime = CLOCK();
  long endnid = NODE_ID();
  if (startnid != endnid) {
    printf("start NODE_ID %d end NODE_ID %d\n", startnid, endnid); exit(1); }
  double time_ms = (double)(endtime - starttime) / (175.0 * 1e3);
  return time_ms;
}

// one nodelet to nodelet ping_pong
void ping_pong_spawn_nlet(long src_nlet, long dst_nlet)
{
  long *srcptr = mw_get_nth(&num_migrations, src_nlet); // get pointers
  long *dstptr = mw_get_nth(&num_migrations, dst_nlet);
  MIGRATE(srcptr); // migrate to source nodelet
  double time_ms = ping_pong_spawn(srcptr, dstptr);
  results[src_nlet][dst_nlet] += time_ms;
}

// iterates over all nlets for a given source or dest (<0 means loop)
void ping_pong_spawn_dist(long src_nlet, long dst_nlet)
{
  for (long nlet = 0; nlet < NODELETS(); ++nlet) {
    if (src_nlet < 0) src_nlet = nlet; // loop over all sources for a dest
    else if (dst_nlet < 0) dst_nlet = nlet; // loop over all dests for a src

    if (dst_nlet == src_nlet) { continue; } // Skip duplicate trials
    ping_pong_spawn_nlet(src_nlet, dst_nlet);
  }
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

int main(int argc, char** argv)
{
  // default src<->dst, log2 migrations (4 per iteration), threads, trials
  long src = 1, dst = 2, log2_num = 3, nth = 2, ntr = 2;
  long debug = 0, use_asm = 0;
  int c;
  while ((c = getopt(argc, argv, "hds1:2:3:4:5:")) != -1) {
    switch (c) {
    case 'h':
      printf("Program options:\n");
      printf("\t-h print this help and exit\n");
      printf("\t-d debug mode [%ld]\n", debug);
      printf("\t-s use assembler [%ld]\n", use_asm);
      printf("\t-1 <N> source nodelet (<0 = all) [%ld]\n", src);
      printf("\t-2 <N> dest nodelet (<0 = all) [%ld]\n", dst);
      printf("\t-3 <N> log2_num_migrations [%ld]\n", log2_num);
      printf("\t-4 <N> number of threads [%ld]\n", nth);
      printf("\t-5 <N> number of trials [%ld]\n", ntr);
      exit(0);
    case 'd': debug = 1; break;
    case 's': use_asm = 1; break;
    case '1': src = atol(optarg); break;
    case '2': dst = atol(optarg); break;
    case '3': log2_num = atol(optarg); break;
    case '4': nth = atol(optarg); break;
    case '5': ntr = atol(optarg); break;
    }
  }
  
  // check bounds on parameters
  if (src >= NODELETS()) { printf("src_nlet too high\n"); exit(1); }
  if (dst >= NODELETS()) { printf("dst_nlet too high\n"); exit(1); }
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

#ifdef DEBUG
  // printf results if in debug mode
  for (long i = 0; i < NODELETS(); ++i) {
    for (long j = 0; j < NODELETS(); ++j) {
      if (results[i][j] > 0) {
	double time_ms = results[i][j] / (double)ntr;
	double migrates_per_sec = (double)(num_migrations * 1e3) / time_ms;
	printf("Average time over trials %f\n", time_ms);
	printf("%3.2f million migrations per second\n",
	       migrates_per_sec / (1e6));
	printf("Latency (amortized): %3.2f us\n",
	       (1.0 / migrates_per_sec) * 1e6);
      }
    }
  }
#endif
  return 0;
}
