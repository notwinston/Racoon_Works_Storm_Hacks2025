#pragma once
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>

// ============================================================================
// NODE CLASS - Enhanced for Research Paper Algorithm
// ============================================================================

class Node {
private:
    std::string name_;                    // The name of the operator node (e.g., "A", "B", "C")
    std::vector<std::string> input_names_; // Input node names (not Node objects to avoid circular dependencies)
    int run_mem_;                         // Memory required for the operator's computation process
    int output_mem_;                      // Memory occupied by the operator's computation result
    int time_cost_;                       // Time taken for the operator's computation
    
    // Research paper attributes (Section 3)
    int peak_;                            // Maximum memory during node execution
    int impact_;                          // Net memory change after execution (output - freed inputs)
    
public:
    // Constructor
    Node(std::string name, std::vector<std::string> inputs, 
         int run_mem, int output_mem, int time_cost);
    
    // Getters
    std::string getName() const { return name_; }
    const std::vector<std::string>& getInputs() const { return input_names_; }
    int getRunMem() const { return run_mem_; }
    int getOutputMem() const { return output_mem_; }
    int getTimeCost() const { return time_cost_; }
    int getPeak() const { return peak_; }
    int getImpact() const { return impact_; }
    
    // Setters
    void setImpact(int impact) { impact_ = impact; }
    
    // Utility methods
    bool hasInput(const std::string& input_name) const;
    int getInputCount() const { return input_names_.size(); }
};

// ============================================================================
// SCHEDULE STATE - Track execution progress
// ============================================================================

struct ScheduleState {
    std::vector<std::string> execution_order;           // Node execution sequence
    int current_memory;                                  // Current memory usage
    int memory_peak;                                     // Peak memory so far
    int total_time;                                      // Total execution time
    std::unordered_set<std::string> computed;           // Already executed nodes
    std::unordered_map<std::string, int> output_memory; // Memory per node output
    
    ScheduleState() : current_memory(0), memory_peak(0), total_time(0) {}
    
    // Utility methods
    void addNode(const std::string& node_name, int time_cost, int output_mem);
    void freeMemory(int memory_amount);
    bool isComputed(const std::string& node_name) const;
};

// ============================================================================
// PARSER CLASS - Parse input files
// ============================================================================

class InputParser {
private:
    std::string filename_;
    int total_memory_;
    std::unordered_map<std::string, Node> nodes_;
    std::unordered_map<std::string, std::vector<std::string>> successors_;
    std::unordered_map<std::string, std::unordered_set<std::string>> dependencies_;
    
public:
    InputParser(const std::string& filename);
    
    // Parse the input file
    bool parse();
    
    // Getters
    int getTotalMemory() const { return total_memory_; }
    const std::unordered_map<std::string, Node>& getNodes() const { return nodes_; }
    const std::unordered_map<std::string, std::vector<std::string>>& getSuccessors() const { return successors_; }
    const std::unordered_map<std::string, std::unordered_set<std::string>>& getDependencies() const { return dependencies_; }
    
    // Utility methods
    void printGraph() const;
    std::vector<std::string> getRootNodes() const;  // Nodes with no inputs
    std::vector<std::string> getLeafNodes() const;  // Nodes with no outputs
};

// ============================================================================
// SCHEDULER CLASS - Main scheduling algorithm
// ============================================================================

class Scheduler {
private:
    const InputParser& parser_;
    bool enable_parallel_;
    bool debug_mode_;
    
    // Research paper algorithms
    int calculateSequentialPeak(const ScheduleState& state, const Node& node_B, int impact_A);
    bool isBetterSchedule(const ScheduleState& state1, const ScheduleState& state2, long total_memory);
    bool canClusterSingleSuccessor(const Node& A, const Node& B);
    bool canClusterSinglePredecessor(const Node& A, const Node& B);
    std::vector<Node> pruneReadyListNegativeOptimization(const std::vector<Node>& ready_list, const ScheduleState& state);
    double scoreRecomputationCandidate(const Node& node, const std::unordered_map<std::string, Node>& all_nodes, const std::unordered_map<std::string, std::vector<std::string>>& successors);
    std::unordered_set<std::string> getFreeableInputs(const Node& node, const ScheduleState& state, const std::unordered_map<std::string, std::unordered_set<std::string>>& dependencies);
    
    // Scheduling algorithms
    ScheduleState scheduleSequential();
    ScheduleState scheduleWithRecomputation();
    std::vector<std::string> getReadyNodes(const ScheduleState& state) const;
    void updateMemoryTracking(ScheduleState& state, const Node& node);
    
public:
    Scheduler(const InputParser& parser, bool enable_parallel = false, bool debug_mode = false);
    
    // Main scheduling method
    ScheduleState findOptimalSchedule();
    
    // Output methods
    void printSchedule(const ScheduleState& state) const;
    void printStatistics(const ScheduleState& state) const;
};