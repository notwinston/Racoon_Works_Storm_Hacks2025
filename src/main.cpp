// ============================================================================
// MAIN PROGRAM - Huawei Challenge Solution
// ============================================================================

#include "node.h"
#include <iostream>
#include <string>
#include <vector>

// ============================================================================
// USAGE AND HELP
// ============================================================================

void printUsage(const char* program_name) {
    std::cout << "Usage: " << program_name << " <input_file> [options]" << std::endl;
    std::cout << "\nOptions:" << std::endl;
    std::cout << "  --parallel     Enable parallel execution (experimental)" << std::endl;
    std::cout << "  --debug        Enable debug output" << std::endl;
    std::cout << "  --help         Show this help message" << std::endl;
    std::cout << "\nExample:" << std::endl;
    std::cout << "  " << program_name << " input/example1.txt --debug" << std::endl;
}

// ============================================================================
// MAIN FUNCTION
// ============================================================================

int main(int argc, char* argv[]) {
    // Parse command line arguments
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }
    
    std::string input_file = argv[1];
    bool enable_parallel = false;
    bool debug_mode = false;
    
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--parallel") {
            enable_parallel = true;
        } else if (arg == "--debug") {
            debug_mode = true;
        } else if (arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else {
            std::cerr << "Unknown option: " << arg << std::endl;
            printUsage(argv[0]);
            return 1;
        }
    }
    
    std::cout << "=== Huawei Challenge: Memory-Constrained Scheduling ===" << std::endl;
    std::cout << "Input file: " << input_file << std::endl;
    std::cout << "Parallel execution: " << (enable_parallel ? "enabled" : "disabled") << std::endl;
    std::cout << "Debug mode: " << (debug_mode ? "enabled" : "disabled") << std::endl;
    
    try {
        // Parse input file
        InputParser parser(input_file);
        if (!parser.parse()) {
            std::cerr << "Failed to parse input file: " << input_file << std::endl;
            return 1;
        }
        
        if (debug_mode) {
            parser.printGraph();
        }
        
        // Create scheduler
        Scheduler scheduler(parser, enable_parallel, debug_mode);
        
        // Find optimal schedule
        ScheduleState optimal_schedule = scheduler.findOptimalSchedule();
        
        // Print results
        scheduler.printSchedule(optimal_schedule);
        scheduler.printStatistics(optimal_schedule);
        
        // Print detailed execution trace if debug mode
        if (debug_mode) {
            std::cout << "\n=== DETAILED EXECUTION TRACE ===" << std::endl;
            int cumulative_time = 0;
            int current_memory = 0;
            int peak_memory = 0;
            
            for (const auto& node_name : optimal_schedule.execution_order) {
                const Node& node = parser.getNodes().at(node_name);
                cumulative_time += node.getTimeCost();
                current_memory += node.getOutputMem();
                peak_memory = std::max(peak_memory, current_memory);
                
                std::cout << "Step " << (optimal_schedule.execution_order.size() - 
                    (std::find(optimal_schedule.execution_order.begin(), 
                               optimal_schedule.execution_order.end(), node_name) - 
                     optimal_schedule.execution_order.begin())) 
                          << ": Execute " << node_name 
                          << " (time=" << node.getTimeCost() 
                          << ", cumulative_time=" << cumulative_time
                          << ", memory=" << current_memory 
                          << ", peak=" << peak_memory << ")" << std::endl;
            }
        }
        
        std::cout << "\n=== SOLUTION SUMMARY ===" << std::endl;
        std::cout << "✅ Optimal execution sequence found!" << std::endl;
        std::cout << "📊 Total time: " << optimal_schedule.total_time << " units" << std::endl;
        std::cout << "💾 Peak memory: " << optimal_schedule.memory_peak << " / " 
                  << parser.getTotalMemory() << " units" << std::endl;
        
        if (optimal_schedule.memory_peak <= parser.getTotalMemory()) {
            std::cout << "🎯 Memory constraint: SATISFIED" << std::endl;
        } else {
            std::cout << "⚠️  Memory constraint: VIOLATED" << std::endl;
            return 1;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
