#include <stdlib.h>
#include <iostream>
#include <algorithm>

#include "graph.hpp"
#include "read.hpp"
#include "boost/graph/dijkstra_shortest_paths_no_color_map.hpp"
#include "heuristics.hpp"
#include "paal_glue.hpp"

#include <signal.h>
extern volatile sig_atomic_t g_stop_signal;

using namespace boost;

template < typename Container, typename Elem >
bool contains(Container c, Elem x) {
	return std::find(c.begin(), c.end(), x) != c.end();
}

//struct EarlyTerminate : public std::exception {};

struct Star {
	Vertex center;
	std::vector<Vertex> terminals;
};

struct Ratio {
	long long weight;
	int terminal_count;
	Weight max_dist;

	bool is_lower_bound() const { return terminal_count < 0; }
	void set_lower_bound(int lb) {
		weight = lb;
		terminal_count = -1;
	}

	int work() const {
		if (terminal_count < 0) return 1;
		return std::max(terminal_count - 1, 0);
	}

	bool operator<(const Ratio& r) const {
		auto diff = weight * r.work() - r.weight * work();
		return diff < 0 || (diff == 0 && terminal_count > r.terminal_count);
	}
	bool operator<(long long x) const { return weight < x*work(); }

	bool operator>(const Ratio& r) { return r < *this; }
	bool operator>(long long x) { return weight > x*work(); }
	bool operator<=(const Ratio& r) { return !(*this > r); }
	bool operator<=(long long x) { return !(*this > x); }
	bool operator>=(const Ratio& r) { return !(*this < r); }
	bool operator>=(long long x) { return !(*this < x);  }

	bool operator==(const Ratio& r) {
		return weight == r.weight && terminal_count == r.terminal_count;
	}
};


Vertex contract_star(Graph& g, Star& s) {
	std::vector<Edge> edges_to_contract;

	s.terminals.push_back(s.center);
	// UGLY DIRTY HACK !!!
	std::swap(s.terminals, g.terminals);
	greedy_2approx(g, std::back_inserter(edges_to_contract));
	std::swap(s.terminals, g.terminals);

	Vertex ret = -1;
	for(auto e : edges_to_contract) {
//    debug_printf("contracted edge %d and %d", e.source(), e.target() );
		ret = g.buy_edge(e);
	}
	return ret;
}

template < bool precise_stars, typename Dist, typename Pred, typename Heap >
Ratio find_best_star_at(const Graph& g, Star& s, Dist& dist, Pred& pred, Heap& heap) {
	if (precise_stars)
		return find_best_star_at__prec(g, s, dist, pred, heap);
	return find_best_star_at__normal(g, s, dist, pred, heap);
}

template < typename Dist, typename Pred, typename Heap >
Ratio find_best_star_at__normal(const Graph& g, Star& s, Dist& dist, Pred&, Heap& heap) {
	Ratio ratio{0, 0, 0};

	debug_printf("Calculating star at %d\n", s.center);
	SimpleDijkstra(g, s.center, dist, dummy, heap, [&](Vertex v){
		//TEST(dijkstra_work++);
		if (ratio.work() >= 1 && ratio < dist[v]) {
			throw EarlyTerminate();
			debug_printf("  early terminate at %d\n", dist[v]);
		}

		/*if (st_ratio < dist[v]) / 2) {
			ratio.set_lower_bound(dist[v]);
			throw EarlyTerminate();
		}*/

		if (g.is_terminal(v)) {
			ratio.weight += dist[v];
			ratio.terminal_count++;
			s.terminals.push_back(v);
			debug_printf("  add terminal %d (dist %d)\n", v, dist[v]);
		}
	});

	return ratio;
}

template < typename Dist, typename Pred, typename Heap >
Ratio find_best_star_at__prec(const Graph& g, Star& s, Dist& dist, Pred& pred, Heap& heap) {
	Weight terminal_found = std::numeric_limits<Weight>::max();
	Ratio ratio{0, 0, 0};
	pred[s.center] = -1;
	std::vector<Vertex> to_buy;

	Star sc = s;
	Ratio rc;
	if (print_debug) {
		PAUSE_DEBUG rc = find_best_star_at__normal(g, sc, dist, dummy, heap);
	}

	debug_printf("Calculating star at %d\n", s.center);
	if (g.is_terminal(s.center)) {
		ratio.terminal_count++;
		debug_printf("  add terminal %d\n", s.center);
	}

	auto add_terminals = [&]() {
		int i = s.terminals.size() - 1;
		for (; i >= 0 && pred[s.terminals[i]] != -1; i--) {
			Vertex w = s.terminals[i];
			while (pred[w] != -1) {
				Vertex u  = pred[w];
				pred[w] = -1;
				to_buy.push_back(w);
				w = u;
			}
			s.terminals.push_back(w);
			ratio.terminal_count++;
			ratio.weight += terminal_found - dist[w];
		}

		ratio.max_dist += terminal_found;
		terminal_found = std::numeric_limits<Weight>::max();
	};

	SimpleDijkstra(g, s.center, dist, pred, heap, [&](Vertex v){
		//TEST(dijkstra_work++);
		if (terminal_found < dist[v]) {
			debug_printf("  phase end at %d\n", dist[v]);
			add_terminals();

			for (Vertex w : to_buy) {
				dist[w] = 0;
				heap.push(w);
			}
			to_buy.clear();

			heap.push(v);

			return;
		}

		if (ratio.work() >= 1 && ratio < dist[v]) {
			debug_printf("  early terminate at %d\n", dist[v]);
			throw EarlyTerminate();
		}

		if (dist[v] > 0 && g.is_terminal(v)) {
			debug_printf("  add terminal %d (dist %d)\n", v, dist[v]);
			s.terminals.push_back(v);
			terminal_found = dist[v];
		}
	});

	if (terminal_found < std::numeric_limits<Weight>::max()) add_terminals();

	if (print_debug) {
		for (Vertex t : sc.terminals) {
			if (!contains(s.terminals, t)) {
				debug_printf("! Terminal %d (dist %d) missing\n", t, dist[t]);
			}
		}
	}

	return ratio;
}


extern int zelikovsky_lvl;
extern bool ratio_check;

template < bool precise_stars >
void contract_till_the_bitter_end(Graph& g) {

	int n = g.vertex_count;

	Ratio inf_ratio;
	inf_ratio.weight = 1;
	inf_ratio.terminal_count = 0;

	std::vector<Ratio> best_ratio_at(n, inf_ratio);
	std::vector<bool> ratio_invalid(n, true);

	Ratio best_ratio;
	int best_ratio_center;

	std::vector<Weight> dist(g.vertex_count);
	HEAP(heap, g.vertex_count, dist[a] < dist[b]);

	Vertex c = -1;
	int round = 0;
	int isolated_counter = 0;
	int invalid_ratio_count = 0;
	int star_size = 0;
	int star_weight = 0;
	TEST(size_t dijkstra_work = 0);
	goto print;

	while (g.terminal_count > CONST_STOP_CONTRACTIONS_AT_NUMBER_TERMINALS) { TIMER_BEGIN {
		{
		debug_printf("Starting round %d\n",round);

		best_ratio = inf_ratio;
		best_ratio_center = n;
		isolated_counter = 0;
		invalid_ratio_count = 0;
		TEST(dijkstra_work = 0);

		std::vector<Vertex> pred(g.vertex_count);

		for (int i = 0; i < n; i++) {
			CHECK_SIGNALS(goto interrupted);

			if( g.degrees[i] == 0) {
				isolated_counter++;
				//fprintf(stderr, "  Vertex %d skipped (contraction orphan)\n", i);
				continue;
			}
			if (ratio_invalid[i] || ratio_check ||
				(best_ratio_at[i] < best_ratio && best_ratio_at[i].is_lower_bound())) {
				invalid_ratio_count++;

				Star s{i, {}};
				Ratio ratio;
				PAUSE_DEBUG ratio = find_best_star_at<precise_stars>(g, s, dist, pred, heap);

				if (!ratio_invalid[i] && !(ratio == best_ratio_at[i]) && !best_ratio_at[i].is_lower_bound()) {
					SimpleDijkstra(g, i, dist, dummy, heap);
					fprintf(stderr, "  Ratio check failed %c at %d: %lld/%d (%d) -> %lld/%d (%d = maxdist (dist[c] =  %d and c,%d)\n",
						ratio < best_ratio_at[i] ? '+' : '-',
						i,
						best_ratio_at[i].weight,
						best_ratio_at[i].terminal_count - 1,
						best_ratio_at[i].max_dist,
						ratio.weight,
						ratio.terminal_count - 1,
						ratio.max_dist,
						dist[c],
						c
						);
				}

				best_ratio_at[i] = ratio;
				ratio_invalid[i] = false;
			}

			if(best_ratio_at[i] < best_ratio) {
				best_ratio_center = i;
				best_ratio = best_ratio_at[i];
			}
		}
		debug_printf("Found %d isolated vertices\n", isolated_counter);
		assert(!best_ratio.is_lower_bound());

		CHECK_SIGNALS(goto interrupted);

		debug_printf("Invalid ratios recomputed: %d\n", invalid_ratio_count);
		debug_printf("Best ratio is %lld/%d (%.1lf), centered at %d\n", best_ratio.weight,
			best_ratio.work(), best_ratio.weight*1.0/best_ratio.work(), best_ratio_center);

		debug_printf("Finding the best star... ");

		pred[best_ratio_center] = -1;
		Star s{best_ratio_center, {}};
		Ratio ratio;
		PAUSE_DEBUG ratio = find_best_star_at<precise_stars>(g, s, dist, pred, heap);
		Assert(ratio == best_ratio, "Expected %lli/%i but got %lli/%i\n",
			best_ratio.weight, best_ratio.work(), ratio.weight, ratio.work());

		// contract star
		debug_printf("Star with %i terminals", ratio.terminal_count);
		star_size = best_ratio.terminal_count;
		star_weight = best_ratio.weight;
		c = contract_star(g, s);
		debug_printf(" centered in %d\n", c);
		assert(c != -1);
		assert(c != -2);

		SimpleDijkstra(g, c, dist, dummy, heap, [&](Vertex v){
			if (best_ratio_at[v] >= dist[v] - best_ratio_at[v].max_dist)
				ratio_invalid[v] = true;
		});
		}

	} TIMER_END("round took: %lg s\n", timer);
		print:
		debug_printf("|V| = %d, |E| = %d, |R| = %d\n", g.vertex_count, g.edge_count, g.terminal_count);

		CHECK_SIGNALS(goto interrupted);

		TEST(
			const auto sol_size = g.partial_solution.size();

			if (zelikovsky_lvl > 0) TIMER_BEGIN {
				zelikovsky(g, std::back_inserter(g.partial_solution), true);
			} TIMER_END("Zelikovsky %lg s\n", timer);
			const auto weight_zelikovsky = g.partial_solution_weight();
			g.partial_solution.resize(sol_size);
			if (zelikovsky_lvl > 1)  TIMER_BEGIN {
				 zelikovsky(g, std::back_inserter(g.partial_solution), false);
			} TIMER_END("Zelikovsky- %lg s\n", timer);
			const auto weight_zelikovsky_ = g.partial_solution_weight();
			Graph sol_zel = g.get_solution();
			g.partial_solution.resize(sol_size);

			Weight orig_w = -1;
			if (zelikovsky_lvl > 0) while (orig_w != sol_zel.partial_solution_weight()) {
				std::vector<Edge> tmp;
				orig_w = sol_zel.partial_solution_weight();
				refine_solution(sol_zel, {}, std::back_inserter(tmp));
				std::swap(tmp, sol_zel.partial_solution);
			}

			greedy_2approx(g, std::back_inserter(g.partial_solution));
			const auto weight_mst = g.partial_solution_weight();
			Graph sol = g.get_solution();
			g.partial_solution.resize(sol_size);

			orig_w = -1;
			while (orig_w != sol.partial_solution_weight()) {
				std::vector<Edge> tmp;
				orig_w = sol.partial_solution_weight();
				refine_solution(sol, {}, std::back_inserter(tmp));
				std::swap(tmp, sol.partial_solution);
			}

			debug_printf("%i,,%i,%i,%i,,%i,%i,,%i,%i,%i,%i,%i,,%i,%i,@@\n\n", round,
				g.vertex_count - isolated_counter, g.edge_count, g.terminal_count,
				invalid_ratio_count, dijkstra_work,
				weight_mst, sol.partial_solution_weight(), weight_zelikovsky,
					weight_zelikovsky_, sol_zel.partial_solution_weight(),
				star_weight, star_size
			);
		)

		// perfom heuristics to clean it before next run
		run_noninvalidating_heuristics(g);
		round++;
	}

	if(CONST_STOP_CONTRACTIONS_AT_NUMBER_TERMINALS > 1)
		greedy_2approx(g, std::back_inserter(g.partial_solution));

	return;

	interrupted:
	debug_printf("%s interrupted\n", __func__);
	greedy_2approx(g, std::back_inserter(g.partial_solution));
}
