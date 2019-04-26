#ifndef PAAL_GLUE_HPP
#define PAAL_GLUE_HPP

#include "graph.hpp"
#include "boost/graph/kruskal_min_spanning_tree.hpp"
#include "boost/range/algorithm/sort.hpp"
#include "boost/range/algorithm/unique.hpp"
#include "boost/range/algorithm/copy.hpp"

// Heavily modified paal::steiner_tree_greedy
template < typename OutputIterator, typename Lambda = Dummy >
void greedy_2approx(const Graph& g, const std::vector<Vertex>& terminals,
		OutputIterator out, Lambda mst_edge_lambda = {}) {
	std::vector<Weight> distance(g.vertex_count, std::numeric_limits<Weight>::max());

	HEAP(heap, g.vertex_count, distance[a] < distance[b]);

	std::vector<Vertex> nearest_terminal(g.vertex_count);
	for (int i = 0; i < (int)terminals.size(); i++) {
		Vertex t = terminals[i];
		if (t < 0) continue;
		nearest_terminal[t] = i;
		distance[t] = 0;
		heap.push(t);
	}

	// compute voronoi diagram each vertex get nearest terminal and last edge on
	// path to nearest terminal
	std::vector<Edge> vpred(g.vertex_count);

	Dijkstra(g, distance, dummy, heap, dummy,
		[&](Edge e) {
			nearest_terminal[e.target()] = nearest_terminal[e.source()];
			vpred[e.target()] = e;
		}, dummy
	);

	// computing distances between terminals
	// creating terminal_graph
	Graph tmp(terminals.size());
	for (int i = 0; i < (int)g.edge_list.size(); i++) {
		auto e = g.edge_list[i];
		Vertex st = nearest_terminal[e.source()];
		Vertex tt = nearest_terminal[e.target()];
		if (st != tt) {
			Weight d = distance[e.source()] + distance[e.target()] + e.weight();
			tmp.add_edge(st, tt, d, EDGE_EXT_REF, i);
		}
	}

	// computing spanning tree on terminal_graph
	std::vector<Edge> terminal_edges;
	boost::kruskal_minimum_spanning_tree(tmp, std::back_inserter(terminal_edges));

	// computing result
	for (auto t_edge : terminal_edges) {
		mst_edge_lambda(t_edge);
		Edge e = g.edge_list[t_edge.orig_edge()];
		*out++ = e;
		for (auto v : { e.source(), e.target() }) {
			while (nearest_terminal[v] != -1 && terminals[nearest_terminal[v]] != v) {
				*out++ = vpred[v];
				nearest_terminal[v] = -1;
				v = vpred[v].source();
			}
		}
	}
}

template < typename OutputIterator >
void greedy_2approx(const Graph& g, OutputIterator out) {
	greedy_2approx(g, g.terminals, out);
}

template < typename T >
struct Vector2D : std::vector<T> {
	int row_size;

	T& at(int x, int y) {
		if (y > x) std::swap(x, y);
		return (*this)[y*row_size + x];
	}

	Vector2D(int size) : std::vector<T>(size*size), row_size(size) {}
};

template < typename T, typename V >
auto get_index(const T& t, const V& v) {
	int i = 0;
	for (const auto& x : t) {
		if (x == v) return i;
		i++;
	}

	assert(0);
}

template < typename OutputIterator >
void zelikovsky(const Graph& _g, OutputIterator out, bool precalculate) {
	Graph g{_g, Graph::copy_tag()};
	std::vector<Vertex> W(g.terminals);

	std::vector<Weight> distance(g.vertex_count);
	std::vector<Vertex> pred(g.vertex_count);
	HEAP(heap, g.vertex_count, distance[a] < distance[b]);

	std::vector<Vertex> terminal_mapping((int)g.terminals.size());
	for (Vertex i = 0; i < (int)g.terminals.size(); i++) terminal_mapping[i] = i;

	std::vector<Vertex> terminals{g.terminals};
	std::vector<std::vector<std::vector<std::pair<Vertex, Weight>>>> triplets;
	const auto triplets_get = [&](Vertex x, Vertex y, Vertex z) -> auto& {
		Vertex verts[3] = { x, y, z };
		std::sort(verts, verts + 3);
		return triplets[verts[2]][verts[1]][verts[0]];
	};

	const auto find_triplets = [&](auto f) {
		for (Vertex center = 0; center < g.vertex_count; center++) {
			if (g.degrees[center] == 0) continue;

			Vertex nearest_terminal = -1;

			SimpleDijkstra(g, center, distance, dummy, heap, [&](Vertex v) {
				if (nearest_terminal < 0 && g.is_terminal(v))
					nearest_terminal = v;
			});

			Vertex u = get_index(g.terminals, nearest_terminal);

			for (int v = 1; v < (int)g.terminals.size(); v++) {
				if (v == u) continue;

				for (int w = 0; w < v; w++) {
					if (w == u) continue;

					f(distance, center, u, v, w);
				}
			}
		}
	};

	if (precalculate) {
		triplets.assign(g.terminals.size(), {});
		for (unsigned i = 0; i < triplets.size(); i++) {
			triplets[i].assign(i, {});

			for (unsigned j = 0; j < triplets[i].size(); j++)
				triplets[i][j].assign(j, { -1 , std::numeric_limits<Weight>::max() });
		}

		find_triplets([&](auto& dist, Vertex c, Vertex u, Vertex v, Vertex w) {
			Weight weight = dist[g.terminals[u]] + dist[g.terminals[v]] + dist[g.terminals[w]];
			if (weight < triplets_get(u, v, w).second) {
				triplets_get(u, v, w) = { c, weight };
			}
		});
	}

	while (g.terminals.size() >= 3) {
		Weight best_win = 0;
		std::vector<Vertex> best_star = { -1, -1, -1, -1 };

		TIMER_BEGIN {

		Graph T{(int)g.terminals.size()};
		greedy_2approx(g, precalculate ? terminals : g.terminals, dummy, [&](Edge e) {
			T.add_edge(e.source(), e.target(), e.weight(), EDGE_EXT_REF, -1);
		});

		Vector2D<Weight> save(g.terminals.size());
		std::vector<Vertex> stack;
		stack.push_back(0);

		while (!stack.empty()) {
			Vertex s = stack.back();
			stack.pop_back();

			EdgeData light_edge;
			light_edge.weight = -1;

			Edge heaviest_edge = Edge(&light_edge);

			DFS(T, s, [&](Vertex, Vertex, Edge e) {
				if (e.weight() > heaviest_edge.weight())
					heaviest_edge = e;
			}, dummy);

			if (heaviest_edge.weight() >= 0) {
				T.remove_edge(heaviest_edge);
				stack.push_back(heaviest_edge.source());
				stack.push_back(heaviest_edge.target());

				DFS(T, heaviest_edge.source(), [&](Vertex v, Vertex, Edge) {
					DFS(T, heaviest_edge.target(), [&](Vertex w, Vertex, Edge) {
						save.at(v, w) = heaviest_edge.weight();
					}, dummy, true);
				}, dummy, true);
			}
		}

		if (precalculate) {
			for (unsigned i = 0; i < triplets.size(); i++) {
				auto tuples = triplets[i];
				for (unsigned j = 0; j < tuples.size(); j++) {
					auto verts = tuples[j];
					for (unsigned k = 0; k < verts.size(); k++) {
						Vertex i_ = terminal_mapping[i];
						Vertex j_ = terminal_mapping[j];
						Vertex k_ = terminal_mapping[k];

						if (i_ == j_ || i_ == k_ || j_ == k_) continue;

						Weight saves[3] = { save.at(i_, j_), save.at(j_, k_), save.at(i_, k_) };
						std::sort(saves, saves + 3);
						Weight win = saves[0] + saves[2] - verts[k].second;

						if (win > best_win) {
							best_win = win;
							best_star = { verts[k].first,
								g.terminals[k_], g.terminals[j_], g.terminals[k_] };
						}
					}
				}
			}
		} else find_triplets([&](auto& dist, Vertex center, Vertex u, Vertex v, Vertex w) {
			Weight saves[3] = { save.at(u, v), save.at(u, w), save.at(v, w) };
			std::sort(saves, saves + 3);

			Weight win = saves[0] + saves[2] - (dist[g.terminals[u]] +
				dist[g.terminals[v]] + dist[g.terminals[w]]);

			if (win > best_win) {
				best_win = win;
				best_star = { center, g.terminals[u], g.terminals[v], g.terminals[w] };
			}
		});
		} TIMER_END(">> %i (%i: %i %i %i) (%lg s)\n", best_win,
			best_star[0], best_star[1], best_star[2], best_star[3], timer);

		if (best_win == 0) break;

		std::vector<Vertex> pred(g.vertex_count, -1);
		SimpleDijkstra(g, best_star[0], distance, pred, heap);

		for (Vertex v : { best_star[1], best_star[2], best_star[3] }) {
			while (v != best_star[0]) {
				Assert(pred[v] != -1, "++ %i\n", v);
				g.find_edge(v, pred[v]).edge_data()->weight = 0;
				v = pred[v];
			}
		}

		if (precalculate) {
			auto u = get_index(terminals, best_star[1]);
			auto v = get_index(terminals, best_star[2]);
			auto w = get_index(terminals, best_star[3]);

			for (auto &x : terminal_mapping) {
				if (x == v || x == w) x = u;
			}

			terminals[v] = -1;
			terminals[w] = -1;
		} else {
			g.unmark_terminal(best_star[2]);
			g.unmark_terminal(best_star[3]);
		}

		W.push_back(best_star[0]);
	}

	greedy_2approx(_g, W, out);
}

#endif
