// ============================================================================
// SCHEDULER IMPLEMENTATION - Research Paper Algorithms
// ============================================================================

#include "node.h"
#include <algorithm>
#include <iostream>
#include <queue>
#include <set>

// ============================================================================
// SCHEDULER CONSTRUCTOR
// ============================================================================

Scheduler::Scheduler(const InputParser& parser, bool enable_parallel, bool debug_mode)
    : parser_(parser), enable_parallel_(enable_parallel), debug_mode_(debug_mode) {}

// ============================================================================
// RESEARCH PAPER ALGORITHMS
// ============================================================================

/**
 * Equation 2: Sequential Composition (Section 3)
 * Calculate memory peak when executing nodes sequentially
 * Formula: (A; B) peak = max(peak_A, peak_B + impact_A)
 */
int Scheduler::calculateSequentialPeak(const ScheduleState& state, const Node& node_B, int impact_A) {
    int peak_B = node_B.getPeak();  // Internal peak during B's execution
    
    // Equation (2) from paper:
    // New peak = max(existing peak, B's peak + accumulated memory)
    return std::max(state.memory_peak, peak_B + impact_A);
}

/**
 * Equation 3 (ADAPTED): Schedule Comparison for Time Minimization
 * Original paper: S1 ⪰_p S2 ⇔ peak1 >= peak2 (minimize peak)
 * Adapted: Minimize TIME subject to memory constraint
 */
bool Scheduler::isBetterSchedule(const ScheduleState& state1, const ScheduleState& state2, 
                                 long total_memory) {
    bool s1_valid = (state1.memory_peak <= total_memory);
    bool s2_valid = (state2.memory_peak <= total_memory);
    
    // Invalid schedules lose
    if (!s1_valid && !s2_valid) return false;
    if (s1_valid && !s2_valid) return true;
    if (!s1_valid && s2_valid) return false;
    
    // Both valid: minimize time first (Huawei primary objective)
    if (state1.total_time != state2.total_time) {
        return state1.total_time < state2.total_time;
    }
    
    // Time tie: minimize peak (Huawei tiebreaker)
    return state1.memory_peak < state2.memory_peak;
}

/**
 * Property 1 / Rule C1: Single successor clustering
 * Condition: i_A >= 0 AND (peak_B + i_A >= peak_A)
 */
bool Scheduler::canClusterSingleSuccessor(const Node& A, const Node& B) {
    int i_A = A.getImpact();
    int p_A = A.getPeak();
    int p_B = B.getPeak();
    
    return (i_A >= 0) && (p_B + i_A >= p_A);
}

/**
 * Property 2 / Rule C2: Single predecessor clustering
 * Condition: i_B <= 0 AND (peak_A >= peak_B + i_A)
 */
bool Scheduler::canClusterSinglePredecessor(const Node& A, const Node& B) {
    int i_B = B.getImpact();
    int i_A = A.getImpact();
    int p_A = A.getPeak();
    int p_B = B.getPeak();
    
    return (i_B <= 0) && (p_A >= p_B + i_A);
}

/**
 * Section 5.2: Negative Node Optimization
 * Prune ready list when negative-impact nodes are available
 */
std::vector<Node> Scheduler::pruneReadyListNegativeOptimization(
    const std::vector<Node>& ready_list, 
    const ScheduleState& state) {
    
    // Find negative-impact node with smallest peak
    const Node* best_negative = nullptr;
    int min_negative_peak = std::numeric_limits<int>::max();
    
    for (const auto& node : ready_list) {
        if (node.getImpact() <= 0 && node.getPeak() < min_negative_peak) {
            best_negative = &node;
            min_negative_peak = node.getPeak();
        }
    }
    
    // No negative nodes? No pruning possible
    if (best_negative == nullptr) {
        return ready_list;
    }
    
    // CASE 1: Negative node doesn't increase peak
    int predicted_peak = calculateSequentialPeak(state, *best_negative, state.current_memory);
    if (predicted_peak <= state.memory_peak) {
        return {*best_negative};  // Return ONLY this node
    }
    
    // CASE 2: Remove nodes with peak >= min_negative_peak
    std::vector<Node> pruned;
    for (const auto& node : ready_list) {
        if (&node == best_negative || node.getPeak() < min_negative_peak) {
            pruned.push_back(node);
        }
    }
    
    return pruned.empty() ? ready_list : pruned;
}

/**
 * Score a node as recomputation candidate
 * Higher score = better recomputation candidate
 */
double Scheduler::scoreRecomputationCandidate(
    const Node& node,
    const std::unordered_map<std::string, Node>& all_nodes,
    const std::unordered_map<std::string, std::vector<std::string>>& successors) {
    
    double score = 0.0;
    
    // High output memory = freeing it creates space for others
    score += static_cast<double>(node.getOutputMem()) * 2.0;
    
    // High time cost = expensive to recompute (penalize)
    score -= static_cast<double>(node.getTimeCost()) * 1.5;
    
    // Check if in cluster (should NOT recompute clustered nodes)
    auto succ_it = successors.find(node.getName());
    if (succ_it != successors.end() && succ_it->second.size() == 1) {
        const Node& successor = all_nodes.at(succ_it->second[0]);
        if (canClusterSingleSuccessor(node, successor)) {
            score -= 1000.0;  // Heavy penalty for breaking clusters
        }
    }
    
    return score;
}

/**
 * Identify which input node outputs can be freed after executing current node
 */
std::unordered_set<std::string> Scheduler::getFreeableInputs(
    const Node& node,
    const ScheduleState& state,
    const std::unordered_map<std::string, std::unordered_set<std::string>>& dependencies) {
    
    std::unordered_set<std::string> freeable;
    
    for (const auto& input_name : node.getInputs()) {
        auto dep_it = dependencies.find(input_name);
        if (dep_it == dependencies.end()) {
            freeable.insert(input_name);
            continue;
        }
        
        // Check if ALL consumers have been computed
        bool all_consumers_done = true;
        for (const auto& consumer : dep_it->second) {
            if (state.computed.find(consumer) == state.computed.end()) {
                all_consumers_done = false;
                break;
            }
        }
        
        if (all_consumers_done) {
            freeable.insert(input_name);
        }
    }
    
    return freeable;
}

// ============================================================================
// SCHEDULING ALGORITHMS
// ============================================================================

/**
 * Get nodes that are ready to execute (all inputs computed)
 */
std::vector<std::string> Scheduler::getReadyNodes(const ScheduleState& state) const {
    std::vector<std::string> ready;
    
    for (const auto& [name, node] : parser_.getNodes()) {
        // Skip if already computed
        if (state.isComputed(name)) continue;
        
        // Check if all inputs are computed
        bool all_inputs_ready = true;
        for (const auto& input_name : node.getInputs()) {
            if (!state.isComputed(input_name)) {
                all_inputs_ready = false;
                break;
            }
        }
        
        if (all_inputs_ready) {
            ready.push_back(name);
        }
    }
    
    return ready;
}

/**
 * Update memory tracking after executing a node
 */
void Scheduler::updateMemoryTracking(ScheduleState& state, const Node& node) {
    // Add node's output memory
    state.current_memory += node.getOutputMem();
    state.output_memory[node.getName()] = node.getOutputMem();
    
    // Update peak memory
    state.memory_peak = std::max(state.memory_peak, state.current_memory);
    
    // Free memory from inputs that are no longer needed
    auto freeable = getFreeableInputs(node, state, parser_.getDependencies());
    for (const auto& input_name : freeable) {
        auto it = state.output_memory.find(input_name);
        if (it != state.output_memory.end()) {
            state.freeMemory(it->second);
            state.output_memory.erase(it);
        }
    }
}

/**
 * Sequential scheduling without recomputation
 */
ScheduleState Scheduler::scheduleSequential() {
    ScheduleState state;
    
    while (state.computed.size() < parser_.getNodes().size()) {
        auto ready_nodes = getReadyNodes(state);
        
        if (ready_nodes.empty()) {
            std::cerr << "Error: No ready nodes found - possible circular dependency" << std::endl;
            break;
        }
        
        // Apply negative node optimization
        std::vector<Node> ready_objects;
        for (const auto& name : ready_nodes) {
            ready_objects.push_back(parser_.getNodes().at(name));
        }
        
        auto pruned = pruneReadyListNegativeOptimization(ready_objects, state);
        
        // Choose best node to execute
        std::string best_node = pruned[0].getName();
        for (const auto& node : pruned) {
            // Check memory constraint
            int predicted_peak = calculateSequentialPeak(state, node, state.current_memory);
            if (predicted_peak <= parser_.getTotalMemory()) {
                best_node = node.getName();
                break;
            }
        }
        
        // Execute the node
        const Node& node = parser_.getNodes().at(best_node);
        state.addNode(best_node, node.getTimeCost(), node.getOutputMem());
        updateMemoryTracking(state, node);
        
        if (debug_mode_) {
            std::cout << "Executed: " << best_node 
                      << " (time=" << node.getTimeCost() 
                      << ", current_mem=" << state.current_memory 
                      << ", peak_mem=" << state.memory_peak << ")" << std::endl;
        }
    }
    
    return state;
}

/**
 * Scheduling with recomputation support
 */
ScheduleState Scheduler::scheduleWithRecomputation() {
    // For now, implement a simple greedy approach with recomputation
    // This can be enhanced with more sophisticated algorithms
    
    ScheduleState best_state;
    best_state.total_time = std::numeric_limits<int>::max();
    
    // Try different recomputation strategies
    std::vector<std::string> candidates;
    for (const auto& [name, node] : parser_.getNodes()) {
        if (node.getInputCount() > 0) { // Can be recomputed
            candidates.push_back(name);
        }
    }
    
    // Sort candidates by recomputation score
    std::sort(candidates.begin(), candidates.end(), 
        [this](const std::string& a, const std::string& b) {
            return scoreRecomputationCandidate(
                parser_.getNodes().at(a), parser_.getNodes(), parser_.getSuccessors()) >
                   scoreRecomputationCandidate(
                parser_.getNodes().at(b), parser_.getNodes(), parser_.getSuccessors());
        });
    
    // Try scheduling with different recomputation strategies
    for (size_t i = 0; i <= candidates.size() && i < 3; ++i) { // Limit to top 3 strategies
        ScheduleState state;
        
        // Mark nodes for recomputation
        std::set<std::string> recompute_set;
        for (size_t j = 0; j < i; ++j) {
            recompute_set.insert(candidates[j]);
        }
        
        // Execute with recomputation
        while (state.computed.size() < parser_.getNodes().size()) {
            auto ready_nodes = getReadyNodes(state);
            
            if (ready_nodes.empty()) break;
            
            // Choose node to execute
            std::string best_node = ready_nodes[0];
            for (const auto& name : ready_nodes) {
                const Node& node = parser_.getNodes().at(name);
                int predicted_peak = calculateSequentialPeak(state, node, state.current_memory);
                if (predicted_peak <= parser_.getTotalMemory()) {
                    best_node = name;
                    break;
                }
            }
            
            const Node& node = parser_.getNodes().at(best_node);
            state.addNode(best_node, node.getTimeCost(), node.getOutputMem());
            updateMemoryTracking(state, node);
            
            // If this node is marked for recomputation, remove it from computed set
            // so it can be recomputed later
            if (recompute_set.count(best_node) > 0) {
                // Mark for potential recomputation (simplified approach)
                // In a full implementation, this would be more sophisticated
            }
        }
        
        // Check if this is a better schedule
        if (isBetterSchedule(state, best_state, parser_.getTotalMemory())) {
            best_state = state;
        }
    }
    
    return best_state;
}

// ============================================================================
// MAIN SCHEDULING METHOD
// ============================================================================

ScheduleState Scheduler::findOptimalSchedule() {
    if (debug_mode_) {
        std::cout << "\n=== STARTING SCHEDULING ===" << std::endl;
        std::cout << "Memory limit: " << parser_.getTotalMemory() << std::endl;
        std::cout << "Parallel execution: " << (enable_parallel_ ? "enabled" : "disabled") << std::endl;
    }
    
    // Try sequential scheduling first
    ScheduleState sequential_state = scheduleSequential();
    
    // Try scheduling with recomputation
    ScheduleState recomputation_state = scheduleWithRecomputation();
    
    // Choose the better schedule
    ScheduleState best_state = isBetterSchedule(sequential_state, recomputation_state, parser_.getTotalMemory()) 
                               ? sequential_state : recomputation_state;
    
    if (debug_mode_) {
        std::cout << "\n=== SCHEDULING COMPLETE ===" << std::endl;
        std::cout << "Sequential time: " << sequential_state.total_time 
                  << ", peak: " << sequential_state.memory_peak << std::endl;
        std::cout << "Recomputation time: " << recomputation_state.total_time 
                  << ", peak: " << recomputation_state.memory_peak << std::endl;
        std::cout << "Best time: " << best_state.total_time 
                  << ", peak: " << best_state.memory_peak << std::endl;
    }
    
    return best_state;
}

// ============================================================================
// OUTPUT METHODS
// ============================================================================

void Scheduler::printSchedule(const ScheduleState& state) const {
    std::cout << "\n=== EXECUTION SCHEDULE ===" << std::endl;
    std::cout << "Execution order: ";
    for (size_t i = 0; i < state.execution_order.size(); ++i) {
        if (i > 0) std::cout << " -> ";
        std::cout << state.execution_order[i];
    }
    std::cout << std::endl;
}

void Scheduler::printStatistics(const ScheduleState& state) const {
    std::cout << "\n=== SCHEDULE STATISTICS ===" << std::endl;
    std::cout << "Total execution time: " << state.total_time << std::endl;
    std::cout << "Peak memory usage: " << state.memory_peak << std::endl;
    std::cout << "Memory limit: " << parser_.getTotalMemory() << std::endl;
    std::cout << "Memory efficiency: " << (100.0 * state.memory_peak / parser_.getTotalMemory()) << "%" << std::endl;
    std::cout << "Nodes executed: " << state.execution_order.size() << std::endl;
    
    if (state.memory_peak > parser_.getTotalMemory()) {
        std::cout << "⚠️  WARNING: Memory limit exceeded!" << std::endl;
    } else {
        std::cout << "✅ Memory constraint satisfied" << std::endl;
    }
}
