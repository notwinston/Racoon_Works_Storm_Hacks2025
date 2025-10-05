#include "parser.hpp"
#include "scheduler.hpp"
#include <fstream>
#include <iostream>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "Usage: scheduler <input_file>\n";
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

    // Use default algorithm with default parameters
    DebugOptions dbg{};
    DebugStats stats{};
    ScheduleState result = scheduleWithDebug(prob, 200000, 5.0, dbg, stats);
    
    if (result.execution_order.size() != prob.nodes.size() || result.memory_peak > prob.total_memory) {
        // Fallbacks: heuristic, then dp+greedy, then beam, then greedy
        result = heuristicSchedule(prob);
        if (result.execution_order.size() != prob.nodes.size() || result.memory_peak > prob.total_memory) {
            std::cout << "Falling back to DP+Greedy schedule\n";
            result = dpGreedySchedule(prob, 3, 8);
        }
        if (result.execution_order.size() != prob.nodes.size() || result.memory_peak > prob.total_memory) {
            std::cout << "Falling back to Beam Search schedule\n";
            result = beamSearchSchedule(prob, 64, 200000);
        }
        if (result.execution_order.size() != prob.nodes.size()) {
            std::cout << "Falling back to Greedy schedule\n";
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
    return 0;
}


