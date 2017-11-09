void timer_start();
long timer_stop();
double timer_calc_bandwidth(long ticks, long bytes);
void timer_print_bandwidth(const char* name, double bytes_per_second);

void
timer_delay_1s();

