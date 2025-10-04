#include <iostream>
#include <fstream>
#include "parser.hpp"
#include "scheduler.hpp"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: scheduler <input_file>" << std::endl;
        return 1;
    }
    
    std::ifstream file(argv[1]);
    if (!file) {
        std::cerr << "Error: Could not open file: " << argv[1] << std::endl;
        return 1;
    }
    
    std::cout << "Processing file: " << argv[1] << std::endl;
    
    // Parse the input file
    long total_memory;
    std::vector<ParsedNodeSpec> specs;
    std::string error;
    
    if (!parseExamplesFormat(file, total_memory, specs, error)) {
        std::cerr << "Parse error: " << error << std::endl;
        return 1;
    }
    
    std::cout << "Memory limit: " << total_memory << std::endl;
    std::cout << "Number of nodes: " << specs.size() << std::endl;
    
    // Build the problem
    Problem prob = buildProblem(total_memory, specs);
    
    // Run the scheduler
    ScheduleState result = scheduleWithLimits(prob, 200000, 2.0);
    
    // Output results
    std::cout << "Schedule (order):" << std::endl;
    for (size_t i = 0; i < result.execution_order.size(); ++i) {
        if (i) std::cout << " -> ";
        std::cout << result.execution_order[i];
    }
    std::cout << std::endl;
    std::cout << "Total time: " << result.total_time << std::endl;
    std::cout << "Memory peak: " << result.memory_peak << " (limit=" << prob.total_memory << ")" << std::endl;
    
    return 0;
}