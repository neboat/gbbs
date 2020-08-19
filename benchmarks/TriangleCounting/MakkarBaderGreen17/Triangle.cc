// This code is part of the project "Theoretically Efficient Parallel Graph
// Algorithms Can Be Fast and Scalable", presented at Symposium on Parallelism
// in Algorithms and Architectures, 2018.
// Copyright (c) 2018 Laxman Dhulipala, Guy Blelloch, and Julian Shun
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all  copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

// Usage:
// numactl -i all ./Triangle -rounds 2 -s -c -m clueweb_sym.bytepda
// flags:
//   required:
//     -s : indicates that the graph is symmetric
//   optional:
//     -m : indicate that the graph should be mmap'd
//     -c : indicate that the graph is compressed
//     -rounds : the number of times to run the algorithm

#include "Triangle.h"
#include "shared.h"

namespace gbbs {
template <class Graph, class UT>
double Dynamic_Triangle_runner(Graph &G, UT &updates, int batch_num,
                               commandLine P) {
  // auto ordering = P.getOptionValue("-ordering", "degree");
  std::cout << "### Application: Dynamic Triangle Counting" << std::endl;
  std::cout << "### Graph: " << P.getArgument(0) << std::endl;
  std::cout << "### Threads: " << num_workers() << std::endl;
  // std::cout << "### n: " << G.n << std::endl;
  // std::cout << "### m: " << G.m << std::endl;
  // std::cout << "### b: " << updates.size() << std::endl;
  // // std::cout << "### Params: ordering=" << ordering << std::endl;
  std::cout << "### ------------------------------------" << std::endl;
  assert(P.getOption("-s"));
  size_t count = 0;
  auto f = [&] (uintE u, uintE v, uintE w) { };
  timer t; t.start();
  count = Dynamic_Triangle(G, updates, f, batch_num, P);
  std::cout << "### Num triangles = " << count << "\n";
  double tt = t.stop();
  std::cout << "### Running Time: " << tt << std::endl;
  return tt;
}

}  // namespace gbbs

#define run_dynamic_app(G, updates, APP, rounds, batch_num)                    \
  auto before_state = gbbs::get_pcm_state();                                   \
  pbbs::timer st;                                                              \
  double total_time = 0.0;                                                     \
  for (size_t r = 0; r < rounds; r++) {                                        \
    total_time += APP(G, updates, batch_num, P);                               \
  }                                                                            \
  auto time_per_iter = total_time / rounds;                                    \
  std::cout << "# time per iter: " << time_per_iter << "\n";                   \
  auto after_state = gbbs::get_pcm_state();                                    \
  gbbs::print_pcm_stats(before_state, after_state, rounds, time_per_iter);     \
  G.del();

/* Macro to generate binary for unweighted graph applications that can ingest
 * only
 * symmetric graph inputs and weighted edge updates input (weight is int type)
 */
#define generate_symmetric_dynamic_main(APP, mutates)                          \
  int main(int argc, char *argv[]) {                                           \
    gbbs::commandLine P(argc, argv, " [-s] <inFile> <updateFile1>");           \
    char *iFile = P.getArgument(1);                                            \
    char *uFile1 = P.getArgument(0);                                           \
    int weighted = P.getOptionIntValue("-w", 1);                               \
    int batch_num = P.getOptionIntValue("-nb", 5);                             \
    bool symmetric = P.getOptionValue("-s");                                   \
    bool compressed = P.getOptionValue("-c");                                  \
    bool mmap = P.getOptionValue("-m");                                        \
    bool mmapcopy = mutates;                                                   \
    if (!symmetric) {                                                          \
      std::cout                                                                \
          << "# The application expects the input graph to be symmetric (-s "  \
             "flag)."                                                          \
          << std::endl;                                                        \
      std::cout << "# Please run on a symmetric input." << std::endl;          \
    }                                                                          \
    size_t rounds = P.getOptionLongValue("-rounds", 1);                        \
    gbbs::pcm_init();                                                          \
    vector<gbbs::gbbs_io::Edge<int>> updates;                                  \
    if (weighted == 0) {                                                       \
      updates = gbbs::gbbs_io::read_weighted_edge_list<int>(uFile1);           \
    } else if (weighted == 1) {                                                \
      updates = gbbs::gbbs_io::read_unweighted_edge_list<int>(uFile1, 1);      \
    } else if (weighted == 2) {                                                \
      updates = gbbs::gbbs_io::read_unweighted_edge_list<int>(uFile1, 0);      \
    } else {                                                                   \
      std::cout << "# wrong  weighted flag. use 0 for weighted, 1 for "        \
                   "inserts , 2 for deletes"                                   \
                << std::endl;                                                  \
    }                                                                          \
    if (compressed) {                                                          \
      auto G = gbbs::gbbs_io::read_compressed_symmetric_graph<pbbslib::empty>( \
          iFile, mmap, mmapcopy);                                              \
      gbbs::alloc_init(G);                                                     \
      run_dynamic_app(G, updates, APP, rounds, batch_num)                      \
    } else {                                                                   \
      auto G = gbbs::gbbs_io::read_unweighted_symmetric_graph(iFile, mmap);    \
      gbbs::alloc_init(G);                                                     \
      run_dynamic_app(G, updates, APP, rounds, batch_num)                      \
    }                                                                          \
    gbbs::alloc_finish();                                                      \
  }

//
//   "Usage: ./Triangle [-trict 0] [-s] [-c] [-w] <inFile> <updateFile1> \n"
//   "Optional arguments:\n"
//   "  -w: 0 if the edge list is weighted with 32-bit integers., 1 if
//   unweighted inserts. 2 if unweighted deletes\n" "  -eg: ignore <inFile>.
//   otherwise inFile is the original graph" "  -trict: triangle counts in
//   <inFile>"

//   "  -n: number of vertices"
//   "  -nb: number of batches"
//   "  -bo: updates start eith [bo]th batch", if [-eg], first [bo] batches are
//   statically counted "  -blocksize: blocksize to use"
// [-trict] [-bo 0] [-nb 5] [-blocksize 5]: inFile is the original graph, has
// trict triangles. start with [bo]th batch
// [-eg] [-n] [-bo 0] [-nb 5] [-blocksize 5]: first [bo] are counted statically,
// start with [bo]th batch
generate_symmetric_dynamic_main(gbbs::Dynamic_Triangle_runner, false);