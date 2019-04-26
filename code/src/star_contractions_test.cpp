#include "magic_constants.hpp"

#include <stdio.h>
#include <signal.h>

#include "read.hpp"
#include "star_contractions.hpp"

volatile sig_atomic_t g_stop_signal = 0;

void
signal_handler(int signal)
{
				g_stop_signal = signal;
}

bool getenv_bool(const char* var, bool def) {
	const char *val = getenv(var);
	if (!val || val[0] == '\0') return def;
	switch (val[0]) {
		case 'Y':
		case 'y':
		case '1':
		case 'T':
		case 't':
			return true;

		case 'N':
		case 'n':
		case '0':
		case 'F':
		case 'f':
			return false;

		default: return def;
	}
}

int getenv_int(const char *var, int def) {
	const char *x = getenv(var);
	return (x && *x != '\0') ? atoi(x) : def;
}

int zelikovsky_lvl;
bool ratio_check;

int main(int argc, char** argv) {
	debug_printf("Version: %s\n", VERSION);

	int seed = argc >= 3 ? atoi(argv[2]) : time(NULL);
	debug_printf("Random seed: %d\n", seed);
	_rand_gen = std::mt19937_64(seed);

	/* register signal handler */
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	debug_printf("Start\n");
	Graph g = graph_from_file(stdin);
	debug_printf("Graph loaded\n");
	debug_printf("|V| = %d, |E| = %d, |R| = %d\n", g.vertex_count, g.edge_count, g.terminal_count);

	debug_printf("First clearance of the input. Calling buy_zero and run_all_heuristics.\n");
	buy_zero(g);
	run_all_heuristics(g);

	g.compress_graph();
	g.save_orig_graph();

	std::vector<Vertex> possible_vertices;
	for (Vertex v = 0; v < g.vertex_count; v++)
		if (g.degrees[v] >= 3) possible_vertices.push_back(v);

	int contraction_timeout = getenv_int("STAR_CONTRACTIONS_TIME", CONST_STAR_CONTRACTIONS_TIME);
	int end_heu_timeout = getenv_int("END_HEU_TIME", std::numeric_limits<int>::max());
	zelikovsky_lvl = getenv_int("ZEL_LVL", 0);
	bool precise_stars = getenv_bool("PRECISE_STARS", 0);
	ratio_check = getenv_bool("RATIO_CHECK", 0);

	debug_printf("### ZEL_LVL=%i PRECISE_STARS=%i\n", zelikovsky_lvl, precise_stars);

	debug_printf("\nCalling `contract_till_the_bitter_end`\n");
	TIMER_BEGIN {
		TIMER(contraction_timeout) {
			if (precise_stars) contract_till_the_bitter_end<1>(g);
			else contract_till_the_bitter_end<0>(g);
		}
	} TIMER_END("contract_till_the_bitter_end: %lg s\n", timer);

	TIMER(end_heu_timeout) {
		Graph tmp = end_heu(g, possible_vertices);
		print_solution(stdout, tmp);
	}
}

