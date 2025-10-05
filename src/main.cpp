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

    // Adaptive parameters based on problem size
    size_t num_nodes = prob.nodes.size();
    size_t max_expansions, beam_width, dp_depth, dp_branch;
    double time_limit;
    
        ScheduleState result;
    
    // Set algorithm parameters based on problem size
    if (num_nodes > 200000) {
        // Ultra-massive problems: Set minimal parameters
        std::cout << "Ultra-massive problem detected (" << num_nodes << " nodes)\n";
        max_expansions = 10;
        beam_width = 1;
        dp_depth = 1;
        dp_branch = 1;
        time_limit = 0.1;
    } else if (num_nodes > 50000) {
        // Very large problems: Conservative parameters
        std::cout << "Very large problem detected (" << num_nodes << " nodes)\n";
        max_expansions = 50;
        beam_width = 1;
        dp_depth = 1;
        dp_branch = 1;
        time_limit = 0.2;
    } else if (num_nodes > 10000) {
        // Large problems: Fast parameters
        std::cout << "Large problem detected (" << num_nodes << " nodes)\n";
        max_expansions = std::min<size_t>(500, num_nodes / 100);
        beam_width = 2;
        dp_depth = 1;
        dp_branch = 2;
        time_limit = 1.0;
    } else if (num_nodes > 1000) {
        // Medium problems: Moderate parameters
        std::cout << "Medium problem detected (" << num_nodes << " nodes)\n";
        max_expansions = std::min<size_t>(10000, num_nodes);
        beam_width = 8;
        dp_depth = 2;
        dp_branch = 4;
        time_limit = 3.0;
    } else {
        // Small problems: Full parameters
        std::cout << "Small problem detected (" << num_nodes << " nodes)\n";
        max_expansions = 200000;
        beam_width = 64;
        dp_depth = 3;
        dp_branch = 8;
        time_limit = 5.0;
    }

    // Streamlined algorithm selection - remove complexity, focus on what works
    
    if (num_nodes > 100000) {
        // Ultra-massive (examples 5,6,7): Use only the fastest possible algorithm
        std::cout << "Ultra-massive problem - using immediate greedy (no complex algorithms)\n";
        result = greedySchedule(prob);
    } else if (num_nodes > 50) {
        // Examples 2,3,4: Use the main algorithm that works
        std::cout << "Using main algorithm (scheduleWithDebug)\n"; 
        DebugOptions dbg{};
        DebugStats stats{};
        result = scheduleWithDebug(prob, max_expansions, time_limit, dbg, stats);
    } else {
        // Very small problems: Simple greedy
        std::cout << "Small problem - using greedy\n";
        result = greedySchedule(prob);
    }
    
    // Simple fallback: if main algorithm fails, try minimal alternatives
    if (result.execution_order.size() != prob.nodes.size()) {
        std::cout << "Main algorithm incomplete, trying heuristic...\n";
        result = heuristicSchedule(prob);
        
        if (result.execution_order.size() != prob.nodes.size()) {
            std::cout << "Heuristic failed, trying greedy as final attempt...\n";  
            result = greedySchedule(prob);
        }
        
        if (result.execution_order.size() != prob.nodes.size()) {
            std::cerr << "No feasible schedule found.\n";
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


