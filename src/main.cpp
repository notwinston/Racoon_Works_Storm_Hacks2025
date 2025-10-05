#include "parser.hpp"
#include "scheduler.hpp"
#include "../Visualization/SimpleDAGVisualizer.h"
#include <fstream>
#include <iostream>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "Usage: scheduler <input_file> [--verbose] [--trace] [--max-expansions N] [--time-limit S]\n";
        return 0;
    }
    std::ifstream fin(argv[1]);
    if (!fin) {
        std::cerr << "Failed to open input: " << argv[1] << "\n";
        return 1;
    }
    long total_memory; std::vector<ParsedNodeSpec> specs; std::string error;
    // Try examples format first
    if (!parseExamplesFormat(fin, total_memory, specs, error)) {
        fin.clear(); fin.seekg(0);
        if (!parseSimpleFormat(fin, total_memory, specs, error)) {
            std::cerr << "Parse error: " << error << "\n";
            return 2;
        }
    }
    Problem prob = buildProblem(total_memory, specs);

    // Parse optional flags
    DebugOptions dbg{}; size_t maxExp = 200000; double tlim = 2.0; size_t beamWidth = 64; size_t dpDepth = 3; size_t dpBranch = 8;
    for (int i = 2; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--verbose") dbg.verbose = true;
        else if (a == "--trace") dbg.trace = true;
        else if (a == "--max-expansions" && i + 1 < argc) { maxExp = std::stoull(argv[++i]); }
        else if (a == "--time-limit" && i + 1 < argc) { tlim = std::stod(argv[++i]); }
        else if (a == "--beam-width" && i + 1 < argc) { beamWidth = std::stoull(argv[++i]); }
        else if (a == "--dp-depth" && i + 1 < argc) { dpDepth = std::stoull(argv[++i]); }
        else if (a == "--dp-branch" && i + 1 < argc) { dpBranch = std::stoull(argv[++i]); }
    }

    DebugStats stats{};
    ScheduleState result = scheduleWithDebug(prob, maxExp, tlim, dbg, stats);
    if (result.execution_order.size() != prob.nodes.size() || result.memory_peak > prob.total_memory) {
        // Fallbacks: heuristic, then dp+greedy, then beam, then greedy
        result = heuristicSchedule(prob);
        if (result.execution_order.size() != prob.nodes.size() || result.memory_peak > prob.total_memory) {
            result = dpGreedySchedule(prob, dpDepth, dpBranch);
        }
        if (result.execution_order.size() != prob.nodes.size() || result.memory_peak > prob.total_memory) {
            result = beamSearchSchedule(prob, beamWidth, maxExp);
        }
        if (result.execution_order.size() != prob.nodes.size()) {
            result = greedySchedule(prob);
        }
        if (result.execution_order.size() != prob.nodes.size()) {
            std::cerr << "No feasible schedule found under memory limit.\n";
            return 3;
        }
    }
    std::cout << "Schedule (order):\n";
    for (size_t i = 0; i < result.execution_order.size(); ++i) {
        if (i) std::cout << " -> ";
        const auto& name = result.execution_order[i];
        bool rc = (i < result.recompute_flags.size()) ? result.recompute_flags[i] : false;
        if (rc) std::cout << name << "*"; else std::cout << name;
    }
    std::cout << "\n* denotes recomputation\n";
    std::cout << "Total time: " << result.total_time << "\n";
    std::cout << "Memory peak: " << result.memory_peak << " (limit=" << prob.total_memory << ")\n";
    
    // Create visualization with input filename
    std::cout << "\n🎨 Generating DAG visualization...\n";
    
    // Extract filename from input path
    std::string input_file = argv[1];
    size_t last_slash = input_file.find_last_of("/\\");
    std::string filename = (last_slash != std::string::npos) ? 
                          input_file.substr(last_slash + 1) : input_file;
    
    // Remove extension if present
    size_t last_dot = filename.find_last_of(".");
    if (last_dot != std::string::npos) {
        filename = filename.substr(0, last_dot);
    }
    
    SimpleDAGVisualizer visualizer("output");
    visualizer.visualizeScheduleState(result, prob, filename);
    
    return 0;
}


