#include <stdlib.h>
#include <cilk.h>
#include <memoryweb.h>

#define LOG(...) fprintf(stdout, __VA_ARGS__); fflush(stdout);

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
    LOG("start NODE_ID %d end NODE_ID %d\n", startnid, endnid);
    exit(1);
  }
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

// gather results
void gather()
{  
  for (long i = 0; i < NODELETS(); ++i) {
    for (long j = 0; j < NODELETS(); ++j) {
      if (results[i][j] > 0) {
	double time_ms = results[i][j];
	LOG("time %f\n", time_ms);
	/*
	time_ms /= ntr;
	double migrates_per_sec = (double)(num_migrations * 1e3) / time_ms;
	LOG("%3.2f million migrations per second\n", migrates_per_sec / (1e6));
	LOG("Latency (amortized): %3.2f us\n", (1.0 / migrates_per_sec) * 1e6);
	*/
      }
    }
  }
}

int main(int argc, char** argv)
{
  if (argc != 6) {
    LOG("Args: src dst log2_num_migrations num_threads num_trials\n");
    LOG("      (<0 for src or dst means all)\n");
    exit(1);
  }

  // get arguments
  long src = atol(argv[1]);
  long dst = atol(argv[2]);
  long log2_num = atol(argv[3]);
  long nth = atol(argv[4]);
  long ntr = atol(argv[5]);

  // check bounds on parameters
  if (src >= NODELETS()) { LOG("src_nlet too high\n"); exit(1); }
  if (dst >= NODELETS()) { LOG("dst_nlet too high\n"); exit(1); }
  if (log2_num <= 1) { LOG("num_migrations must be >= 4\n"); exit(1); }
  if (nth <= 0) { LOG("num_threads must be > 0\n"); exit(1); }
  if (ntr <= 0) { LOG("num_trials must be > 0\n"); exit(1); }

  // log variables for the run
  long n = 1L << log2_num;
  LOG("ping pong: num nodelets %d\n", NODELETS());
  LOG("ping pong: src nlet %d\n", src);
  LOG("ping pong: dst nlet %d\n", dst);
  LOG("ping pong: num migrations %d\n", n);
  LOG("ping pong: num threads %d\n", nth);
  LOG("ping pong: num trials %d\n", ntr);

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
  gather();  
  return 0;
}
