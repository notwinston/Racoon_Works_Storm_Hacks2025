# Huawei Challenge: Memory-Constrained Scheduling Algorithm

## Overview

This implementation applies research paper algorithms from "Sequential Scheduling of Dataflow Graphs for Memory Peak Minimization" to solve the Huawei challenge of finding the fastest execution order within memory constraints, with support for recomputation.

## Node Class Design

### Core Structure

```cpp
class Node {
private:
    std::string name_;                    // Node identifier (e.g., "0", "1", "2")
    std::vector<std::string> input_names_; // Input dependencies (by name)
    int run_mem_;                         // Memory required during computation
    int output_mem_;                      // Memory occupied by result
    int time_cost_;                       // Time taken for computation
    
    // Research paper attributes
    int peak_;                            // Maximum memory during execution
    int impact_;                          // Net memory change (output - freed inputs)
};
```

### Key Design Decisions

1. **String-based Dependencies**: Uses `std::vector<std::string>` for input names instead of `std::vector<Node>` to avoid circular dependencies and enable flexible graph construction.

2. **Research Paper Integration**: Incorporates `peak_` and `impact_` attributes from the research paper for advanced scheduling optimizations.

3. **Memory Model**: Implements the PBC (Produce Before Consume) model where nodes produce output before consuming inputs, affecting peak memory calculation.

## Input Parser

### Format Support

The parser handles two input formats:

#### Format 1: Simple Format (sample.txt)
```
total_memory: 350
node A 50 100 10 inputs=-
node B 100 100 8 inputs=A
node C 150 100 5 inputs=A
node D 80 100 6 inputs=B,C
```

#### Format 2: Complex Format (example1.txt, example2.txt, etc.)
```
Return 42467328
0 ExpandDims-op0 0 0 524288 59
1 Sub-op0 1 0 0 524288 525
2 Mul-op0 1 1 0 524288 691
...
```

### Parser Features

- **Variadic Input Support**: Handles nodes with different numbers of input dependencies
- **Dependency Graph Building**: Automatically constructs successor and dependency maps
- **Impact Calculation**: Computes accurate memory impact values based on when inputs can be freed

## Research Paper Algorithms

### Equation 2: Sequential Composition
```cpp
int calculateSequentialPeak(const ScheduleState& state, const Node& node_B, int impact_A) {
    int peak_B = node_B.getPeak();
    return std::max(state.memory_peak, peak_B + impact_A);
}
```

**Purpose**: Predict memory peak before executing a node to check memory constraints.

### Equation 3 (Adapted): Schedule Comparison
```cpp
bool isBetterSchedule(const ScheduleState& state1, const ScheduleState& state2, long total_memory) {
    // 1. Both must satisfy memory constraint
    // 2. Prefer minimum total_time (PRIMARY OBJECTIVE)
    // 3. If times equal, prefer minimum memory_peak (TIEBREAKER)
}
```

**Adaptation**: Modified from peak minimization to time minimization while respecting memory constraints.

### Properties 1 & 2: Clustering Detection
```cpp
bool canClusterSingleSuccessor(const Node& A, const Node& B);
bool canClusterSinglePredecessor(const Node& A, const Node& B);
```

**Purpose**: Identify nodes that must execute consecutively to avoid breaking optimal clusters.

### Section 5.2: Negative Node Optimization
```cpp
std::vector<Node> pruneReadyListNegativeOptimization(const std::vector<Node>& ready_list, const ScheduleState& state);
```

**Strategy**: When memory is tight, prioritize negative-impact nodes that free memory, dramatically reducing search space.

## Scheduling Algorithm

### Core Strategy

1. **Sequential Scheduling**: Basic greedy approach respecting dependencies
2. **Recomputation Support**: Allows re-executing nodes to free memory
3. **Memory Tracking**: Accurate peak calculation with freed memory accounting
4. **Optimization Pruning**: Uses research paper optimizations to reduce search space

### Recomputation Scoring
```cpp
double scoreRecomputationCandidate(const Node& node, ...) {
    double score = 0.0;
    score += node.getOutputMem() * 2.0;        // High output memory = good
    score -= node.getTimeCost() * 1.5;         // High time cost = bad
    // Penalty for breaking clusters
    if (in_cluster) score -= 1000.0;
    return score;
}
```

**Heuristic**: Prioritizes nodes with high memory output and low time cost for recomputation.

## Parallel Execution Support

### Implementation
- **Debug Flag**: `--debug` enables detailed execution traces
- **Parallel Flag**: `--parallel` enables experimental parallel execution
- **Memory Accounting**: Parallel operations still observe memory constraints

### Parallel Considerations
- All parallel operations in a step must fit within memory budget
- Dependencies must still be respected
- May not always save time due to memory pressure

## Usage

### Command Line Interface
```bash
./algorithms input/example1.txt --debug
./algorithms input/example2.txt --parallel
./algorithms sample.txt --help
```

### Program Output
```
=== EXECUTION SCHEDULE ===
Execution order: 0 -> 1 -> 2 -> 3 -> ...

=== SCHEDULE STATISTICS ===
Total execution time: 12345
Peak memory usage: 300 / 350
Memory efficiency: 85.7%
Nodes executed: 82
✅ Memory constraint satisfied
```

## Evaluation Criteria Alignment

### Primary Objective: Minimize Total Execution Time
- All algorithms prioritize time minimization
- Schedule comparison uses time as primary criterion

### Constraint: Memory Limit
- Memory constraint checking before each node execution
- Invalid schedules (exceeding memory) are rejected

### Tiebreaker: Minimize Memory Peak
- When execution times are equal, prefer lower memory usage
- Helps find more efficient solutions

## AI Assistant Integration

### Design Approach
- **Agent Definition**: AI assistant acts as algorithm designer and implementer
- **Prompt Engineering**: Used structured prompts to implement research paper equations
- **Validation**: AI-generated code validated against known examples and constraints

### Implementation Process
1. **Research Paper Analysis**: AI analyzed the paper's mathematical foundations
2. **Algorithm Translation**: Converted equations to C++ implementations
3. **Integration**: Combined multiple algorithms into cohesive scheduling system
4. **Testing**: Validated against provided examples and edge cases

## Key Features

✅ **Research Paper Integration**: Implements core equations and optimizations  
✅ **Recomputation Support**: Allows strategic node re-execution  
✅ **Memory Constraint Satisfaction**: Ensures solutions fit within memory budget  
✅ **Time Optimization**: Primary focus on minimizing execution time  
✅ **Debug Support**: Detailed tracing and statistics  
✅ **Parallel Execution**: Optional parallel processing with memory accounting  
✅ **Flexible Input**: Supports multiple input formats  
✅ **Comprehensive Output**: Detailed execution traces and statistics  

## Future Enhancements

- **Advanced Parallel Scheduling**: More sophisticated parallel execution strategies
- **Dynamic Programming**: Full optimal solution for smaller graphs
- **Machine Learning**: Learn optimal recomputation strategies from examples
- **Memory Profiling**: Detailed memory usage analysis and optimization
