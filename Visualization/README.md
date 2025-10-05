# DAG Visualization Helper Function

A clean, essential helper function that takes your algorithm's execution order and creates visual representations showing which nodes were recomputed.

## 🎯 Purpose

This tool visualizes the output of your Huawei challenge algorithm, specifically showing:
- **Execution order** of nodes
- **Which nodes were recomputed** (like 'A' and 'A'')
- **Dependencies** between nodes
- **Memory usage** and **execution time**

## 🆕 NEW: Direct Integration with Your Scheduler

The visualizer now works directly with your `ScheduleState` and `Problem` structures from your scheduler algorithm!

### 🚀 Simple Integration (Recommended)

Use `SimpleDAGVisualizer` for easy integration with your existing scheduler code.

## 📁 Essential Files

### Simple Integration (Recommended)
- **`SimpleDAGVisualizer.h`** - Simple visualization class
- **`SimpleDAGVisualizer.cpp`** - Implementation
- **`test_simple_integration.cpp`** - Test script
- **`build_simple_integration.bat`** - Build script

### Advanced Integration
- **`Node.h`** - Node class with recomputation support
- **`DAGVisualizer.h`** - Main visualization class
- **`DAGVisualizer.cpp`** - Implementation
- **`test_dag_sizes.cpp`** - Test script with 3 different DAG sizes
- **`build_test.bat`** - Build script for testing
- **`README.md`** - This documentation

## 🚀 Usage

### 1. Simple Integration (Recommended)
```cpp
#include "SimpleDAGVisualizer.h"

// Your scheduler algorithm
ScheduleState result = greedySchedule(problem);

// Add visualization (just 2 lines!)
SimpleDAGVisualizer visualizer("output");
visualizer.visualizeScheduleState(result, problem, "solution");
```

### 2. Manual Node Creation (Original Method)
```cpp
#include "DAGVisualizer.h"

// Your algorithm finds optimal execution order
std::vector<Node> optimal_order = yourOptimizationAlgorithm();

// Add visualization (just 3 lines!)
DAGVisualizer visualizer("output");
visualizer.visualizeExecutionOrder(optimal_order, "solution");
```

### 3. Build and Test
```bash
# Compile (with simple integration)
g++ -std=c++17 -I. -I../include -I../src your_file.cpp SimpleDAGVisualizer.cpp ../src/parser.cpp ../src/scheduler.cpp -o your_program.exe

# Create images (automatic with new integration!)
# The visualizer automatically generates PNG files
```

## 💻 Integration Example

```cpp
// Your existing scheduler algorithm
ScheduleState result = greedySchedule(problem);

// Add visualization (NEW: Simple integration!)
SimpleDAGVisualizer visualizer("results");
visualizer.visualizeScheduleState(result, problem, "optimal_solution");

// Generated files:
// - results/optimal_solution.dot (Graphviz file)
// - results/optimal_solution_summary.txt (Analysis)
// - results/optimal_solution.png (Visual image - auto-generated!)
```

## 🎨 Visual Features

- **Box nodes**: Regular executions
- **Ellipse nodes**: Recomputed nodes (red color)
- **Arrows**: Dependencies and data flow
- **Labels**: Memory, time, and node information

## 🎯 Perfect for Hackathon

- **Debug** your algorithm's recomputation decisions
- **Present** your solution with visual proof
- **Analyze** memory vs time trade-offs
- **Impression judges** with professional visualizations

This helper function takes your algorithm's output and creates visual proof of which nodes were recomputed and why!