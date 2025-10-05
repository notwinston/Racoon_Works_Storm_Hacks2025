#include "scheduler.hpp"
#include <chrono>
#include <iostream>
#include <algorithm>
#include <map>
#include <set>
#include "gurobi_c++.h"

// Bring in implementations from the previous reference file
// Only include what's necessary here

int calculateSequentialPeak(const ScheduleState& state, const Node& node_B, int impact_A) {
    int peak_B = node_B.getPeak();
    return std::max(state.memory_peak, peak_B + impact_A);
}

bool isBetterSchedule(const ScheduleState& state1, const ScheduleState& state2, long total_memory) {
    bool s1_valid = (state1.memory_peak <= total_memory);
    bool s2_valid = (state2.memory_peak <= total_memory);
    if (!s1_valid && !s2_valid) return false;
    if (s1_valid && !s2_valid) return true;
    if (!s1_valid && s2_valid) return false;
    if (state1.total_time != state2.total_time) return state1.total_time < state2.total_time;
    return state1.memory_peak < state2.memory_peak;
}

std::unordered_set<std::string> getFreeableInputs(
    const Node& node,
    const ScheduleState& state,
    const std::unordered_map<std::string, std::unordered_set<std::string>>& dependencies) {
    std::unordered_set<std::string> freeable;
    for (const auto& input_name : node.getInputs()) {
        auto dep_it = dependencies.find(input_name);
        if (dep_it == dependencies.end()) { freeable.insert(input_name); continue; }
        bool all_consumers_done = true;
        for (const auto& consumer : dep_it->second) {
            if (state.computed.find(consumer) == state.computed.end()) { all_consumers_done = false; break; }
        }
        if (all_consumers_done) freeable.insert(input_name);
    }
    return freeable;
}

static std::vector<std::string> getReadyNodeNames(const Problem& prob, const ScheduleState& state) {
    std::vector<std::string> ready;
    ready.reserve(prob.nodes.size());
    for (const auto& kv : prob.nodes) {
        const std::string& name = kv.first;
        if (state.computed.count(name)) continue; // do not schedule original run again here
        const Node& n = kv.second;
        bool ok = true;
        for (const auto& inName : n.getInputs()) {
            // Require the input output to be currently available in memory
            if (state.output_memory.find(inName) == state.output_memory.end()) { ok = false; break; }
        }
        if (ok) ready.push_back(name);
    }
    return ready;
}

static int calculateDynamicImpact(
    const Node& node,
    const ScheduleState& state,
    const std::unordered_map<std::string, std::unordered_set<std::string>>& dependencies,
    const std::unordered_map<std::string, int>& output_memory) {
    ScheduleState postState = state;
    postState.computed.insert(node.getName());
    auto freeable = getFreeableInputs(node, postState, dependencies);
    long freed = 0;
    for (const auto& input_name : freeable) {
        auto it = output_memory.find(input_name);
        if (it != output_memory.end()) freed += it->second;
    }
    long impact = static_cast<long>(node.getOutputMem()) - freed;
    if (impact < 0) return static_cast<int>(impact);
    if (impact > std::numeric_limits<int>::max()) impact = std::numeric_limits<int>::max();
    return static_cast<int>(impact);
}

static ScheduleState executeNode(
    const std::string& node_name,
    const Problem& prob,
    const ScheduleState& state) {
    ScheduleState next = state;
    const Node& node = prob.nodes.at(node_name);
    int predicted_peak = calculateSequentialPeak(state, node, state.current_memory);
    next.memory_peak = std::max(state.memory_peak, predicted_peak);
    ScheduleState postStateForFree = state;
    postStateForFree.computed.insert(node.getName());
    auto freeable = getFreeableInputs(node, postStateForFree, prob.dependencies);
    long freed = 0;
    for (const auto& nm : freeable) {
        auto it = next.output_memory.find(nm);
        if (it != next.output_memory.end()) { freed += it->second; next.output_memory.erase(it); }
    }
    long impact = static_cast<long>(node.getOutputMem()) - freed;
    long new_current = static_cast<long>(next.current_memory) + impact;
    if (new_current < 0) new_current = 0;
    next.current_memory = static_cast<int>(new_current);
    next.total_time += node.getTimeCost();
    next.output_memory[node.getName()] = node.getOutputMem();
    next.execution_order.push_back(node.getName());
    // recompute flag: true if this node was already computed before and we are running again to restore its output
    bool isRecompute = (state.computed.find(node.getName()) != state.computed.end());
    next.recompute_flags.push_back(isRecompute);
    next.computed.insert(node.getName());
    return next;
}

static std::vector<std::string> pruneReadyListDynamic(
    const std::vector<std::string>& ready_names,
    const Problem& prob,
    const ScheduleState& state) {
    const Node* best_negative = nullptr;
    std::string best_name;
    int min_negative_peak = std::numeric_limits<int>::max();
    for (const auto& nm : ready_names) {
        const Node& node = prob.nodes.at(nm);
        int dynImpact = calculateDynamicImpact(node, state, prob.dependencies, state.output_memory);
        if (dynImpact <= 0 && node.getPeak() < min_negative_peak) {
            best_negative = &node; best_name = nm; min_negative_peak = node.getPeak();
        }
    }
    if (best_negative == nullptr) return ready_names;
    int predicted_peak = calculateSequentialPeak(state, *best_negative, state.current_memory);
    if (predicted_peak <= state.memory_peak) return {best_name};
    std::vector<std::string> pruned;
    for (const auto& nm : ready_names) {
        const Node& node = prob.nodes.at(nm);
        if (&node == best_negative || node.getPeak() < min_negative_peak) pruned.push_back(nm);
    }
    return pruned.empty() ? ready_names : pruned;
}

// Recompute candidates: nodes whose output is currently missing but needed by some uncomputed consumer,
// and whose inputs are available in memory now. We allow recomputing even if they ran before.
static std::vector<std::string> getRecomputeCandidates(const Problem& prob, const ScheduleState& state) {
    std::vector<std::string> cands;
    cands.reserve(prob.nodes.size());
    for (const auto& kv : prob.nodes) {
        const std::string& name = kv.first;
        // Skip if output already available
        if (state.output_memory.find(name) != state.output_memory.end()) continue;
        // Must have at least one consumer not yet computed
        auto itSucc = prob.successors.find(name);
        bool needed = false;
        if (itSucc != prob.successors.end()) {
            for (const auto& cons : itSucc->second) {
                if (state.computed.find(cons) == state.computed.end()) { needed = true; break; }
            }
        }
        if (!needed) continue;
        // Inputs for this node must be available to recompute now
        const Node& n = kv.second;
        bool inputsAvail = true;
        for (const auto& inName : n.getInputs()) {
            if (state.output_memory.find(inName) == state.output_memory.end()) { inputsAvail = false; break; }
        }
        if (!inputsAvail) continue;
        cands.push_back(name);
    }
    return cands;
}

// Spill: remove the largest resident output to reduce current memory
static bool trySpillLargest(ScheduleState& state) {
    if (state.output_memory.empty()) return false;
    auto it = std::max_element(state.output_memory.begin(), state.output_memory.end(),
                               [](const auto& a, const auto& b){ return a.second < b.second; });
    if (it == state.output_memory.end()) return false;
    int sz = it->second;
    state.output_memory.erase(it);
    state.current_memory = std::max(0, state.current_memory - sz);
    return true;
}

// Spill with heuristic: pick resident output maximizing (size / (recompute_time+1)) and with remaining consumers
static bool trySpillBest(const Problem& prob, ScheduleState& state) {
    std::string best; double bestScore = -1.0; int bestSize = 0;
    for (const auto& kv : state.output_memory) {
        const std::string& name = kv.first; int sz = kv.second;
        auto itNode = prob.nodes.find(name); if (itNode == prob.nodes.end()) continue;
        int t = std::max(1, itNode->second.getTimeCost());
        // Count remaining consumers
        int remaining = 0; auto itSucc = prob.successors.find(name);
        if (itSucc != prob.successors.end()) {
            for (const auto& cons : itSucc->second) if (state.computed.find(cons) == state.computed.end()) ++remaining;
        }
        if (remaining == 0) {
            // Not needed anymore; just drop it for free
            state.current_memory = std::max(0, state.current_memory - sz);
            // defer erase until after loop to avoid iterator invalidation; mark by size 0
            // But easier: erase now using a separate iterator pattern
        }
        double score = static_cast<double>(sz) / static_cast<double>(t);
        if (score > bestScore) { bestScore = score; best = name; bestSize = sz; }
    }
    if (!best.empty()) {
        state.output_memory.erase(best);
        state.current_memory = std::max(0, state.current_memory - bestSize);
        return true;
    }
    return false;
}

// Garbage-collect outputs that have no remaining consumers
static void garbageCollectOutputs(const Problem& prob, ScheduleState& state) {
    std::vector<std::string> toErase;
    for (const auto& kv : state.output_memory) {
        const std::string& name = kv.first;
        auto itSucc = prob.successors.find(name);
        bool needed = false;
        if (itSucc != prob.successors.end()) {
            for (const auto& cons : itSucc->second) {
                if (state.computed.find(cons) == state.computed.end()) { needed = true; break; }
            }
        }
        if (!needed) toErase.push_back(name);
    }
    for (const auto& name : toErase) {
        auto it = state.output_memory.find(name);
        if (it != state.output_memory.end()) {
            state.current_memory = std::max(0, state.current_memory - it->second);
            state.output_memory.erase(it);
        }
    }
}

static void dfsSchedule(const Problem& prob, ScheduleState& current, ScheduleState& best, bool& has_best) {
    if (current.computed.size() == prob.nodes.size()) {
        if (!has_best || isBetterSchedule(current, best, prob.total_memory)) { best = current; has_best = true; }
        return;
    }
    // Opportunistic GC to tighten memory before expansion
    garbageCollectOutputs(prob, current);
    auto ready = getReadyNodeNames(prob, current);
    if (ready.empty()) return;
    ready = pruneReadyListDynamic(ready, prob, current);
    for (const auto& nm : ready) {
        const Node& node = prob.nodes.at(nm);
        int predicted_peak = calculateSequentialPeak(current, node, current.current_memory);
        if (predicted_peak > prob.total_memory) continue;
        ScheduleState next = executeNode(nm, prob, current);
        dfsSchedule(prob, next, best, has_best);
    }
}

ScheduleState schedule(const Problem& prob) {
    ScheduleState init;
    ScheduleState best; bool has_best = false;
    dfsSchedule(prob, init, best, has_best);
    return best;
}

static void dfsScheduleLimited(const Problem& prob, ScheduleState& current, ScheduleState& best, bool& has_best,
                               size_t& expansionsLeft, const std::chrono::steady_clock::time_point& deadline,
                               const DebugOptions* dbg, DebugStats* stats) {
    if (std::chrono::steady_clock::now() > deadline || expansionsLeft == 0) return;
    if (current.computed.size() == prob.nodes.size()) {
        if (!has_best || isBetterSchedule(current, best, prob.total_memory)) { best = current; has_best = true; }
        return;
    }
    auto ready = getReadyNodeNames(prob, current);
    if (ready.empty()) {
        // Consider recomputation of needed but spilled outputs
        ready = getRecomputeCandidates(prob, current);
        if (ready.empty()) { if (stats) stats->deadEnds++; return; }
    }
    ready = pruneReadyListDynamic(ready, prob, current);
    // If all candidates exceed memory, attempt a spill and retry once
    bool allExceed = true;
    for (const auto& nm : ready) {
        const Node& node = prob.nodes.at(nm);
        int predicted_peak = calculateSequentialPeak(current, node, current.current_memory);
        if (predicted_peak <= prob.total_memory) { allExceed = false; break; }
    }
    if (allExceed) {
        ScheduleState spilled = current;
        if (trySpillBest(prob, spilled) || trySpillLargest(spilled)) {
            dfsScheduleLimited(prob, spilled, best, has_best, expansionsLeft, deadline, dbg, stats);
        }
        return;
    }
    for (const auto& nm : ready) {
        if (std::chrono::steady_clock::now() > deadline || expansionsLeft == 0) return;
        const Node& node = prob.nodes.at(nm);
        int predicted_peak = calculateSequentialPeak(current, node, current.current_memory);
        if (predicted_peak > prob.total_memory) { if (stats) stats->prunedByMemory++; continue; }
        ScheduleState next = executeNode(nm, prob, current);
        --expansionsLeft; if (stats) stats->expansions++;
        if (dbg && dbg->trace) {
            std::cerr << "expand: " << nm << " time=" << next.total_time
                      << " curMem=" << next.current_memory << " peak=" << next.memory_peak
                      << " readyCount=" << ready.size() << " left=" << expansionsLeft << "\n";
        }
        dfsScheduleLimited(prob, next, best, has_best, expansionsLeft, deadline, dbg, stats);
    }
}

ScheduleState scheduleWithLimits(const Problem& prob, size_t maxExpansions, double timeLimitSeconds) {
    ScheduleState init;
    ScheduleState best; bool has_best = false;
    if (maxExpansions == 0) maxExpansions = 100000;
    if (timeLimitSeconds <= 0.0) timeLimitSeconds = 2.0;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
        std::chrono::duration<double>(timeLimitSeconds));
    dfsScheduleLimited(prob, init, best, has_best, maxExpansions, deadline, nullptr, nullptr);
    return has_best ? best : ScheduleState{};
}

ScheduleState greedySchedule(const Problem& prob) {
    ScheduleState cur;
    // Simple greedy: repeatedly pick any ready node minimizing predicted peak, then time
    while (cur.computed.size() < prob.nodes.size()) {
        auto ready = getReadyNodeNames(prob, cur);
        if (ready.empty()) break;
        std::string bestName; int bestPredPeak = std::numeric_limits<int>::max(); int bestTime = std::numeric_limits<int>::max();
        for (const auto& nm : ready) {
            const Node& node = prob.nodes.at(nm);
            int predicted_peak = calculateSequentialPeak(cur, node, cur.current_memory);
            if (predicted_peak > prob.total_memory) continue;
            int t = node.getTimeCost();
            if (predicted_peak < bestPredPeak || (predicted_peak == bestPredPeak && t < bestTime)) {
                bestPredPeak = predicted_peak; bestTime = t; bestName = nm;
            }
        }
        if (bestName.empty()) break;
        cur = executeNode(bestName, prob, cur);
    }
    return cur;
}

// Heuristic schedule: prioritize negative-impact nodes first; otherwise minimize (peak, time)
ScheduleState heuristicSchedule(const Problem& prob) {
    ScheduleState cur;
    while (cur.computed.size() < prob.nodes.size()) {
        auto ready = getReadyNodeNames(prob, cur);
        if (ready.empty()) break;
        std::string bestName; int bestPredPeak = std::numeric_limits<int>::max(); int bestTime = std::numeric_limits<int>::max(); bool pickedNegative = false;
        for (const auto& nm : ready) {
            const Node& node = prob.nodes.at(nm);
            int predicted_peak = calculateSequentialPeak(cur, node, cur.current_memory);
            if (predicted_peak > prob.total_memory) continue;
            int dynImpact = calculateDynamicImpact(node, cur, prob.dependencies, cur.output_memory);
            if (dynImpact <= 0) {
                if (!pickedNegative || node.getPeak() < prob.nodes.at(bestName).getPeak()) { bestName = nm; pickedNegative = true; }
                continue;
            }
            if (pickedNegative) continue;
            int t = node.getTimeCost();
            if (predicted_peak < bestPredPeak || (predicted_peak == bestPredPeak && t < bestTime)) {
                bestPredPeak = predicted_peak; bestTime = t; bestName = nm;
            }
        }
        if (bestName.empty()) break;
        cur = executeNode(bestName, prob, cur);
    }
    return cur;
}

// Beam search: keep top-K partial schedules by (validity, time, peak)
ScheduleState beamSearchSchedule(const Problem& prob, size_t beamWidth, size_t maxExpansions) {
    if (beamWidth == 0) beamWidth = 32; if (maxExpansions == 0) maxExpansions = 200000;
    struct Entry { ScheduleState state; bool operator<(const Entry& other) const {
        // We want a min-heap style comparison; but for vector sort we'll use explicit key
        return false; } };
    std::vector<ScheduleState> beam; beam.reserve(beamWidth);
    beam.push_back(ScheduleState{});
    size_t expansions = 0;
    ScheduleState best; bool has_best = false;
    while (!beam.empty() && expansions < maxExpansions) {
        std::vector<ScheduleState> nextBeam;
        for (const auto& cur : beam) {
            if (cur.computed.size() == prob.nodes.size()) {
                if (!has_best || isBetterSchedule(cur, best, prob.total_memory)) { best = cur; has_best = true; }
                continue;
            }
            auto ready = getReadyNodeNames(prob, cur);
            if (ready.empty()) continue;
            // Sort candidates by predicted peak then time
            std::vector<std::pair<std::string, std::pair<int,int>>> cands;
            for (const auto& nm : ready) {
                const Node& node = prob.nodes.at(nm);
                int p = calculateSequentialPeak(cur, node, cur.current_memory);
                if (p > prob.total_memory) continue;
                cands.push_back({nm, {p, node.getTimeCost()}});
            }
            std::sort(cands.begin(), cands.end(), [](const auto& a, const auto& b){
                if (a.second.first != b.second.first) return a.second.first < b.second.first;
                return a.second.second < b.second.second;
            });
            size_t expandCount = std::min(cands.size(), beamWidth);
            for (size_t i = 0; i < expandCount && expansions < maxExpansions; ++i) {
                nextBeam.push_back(executeNode(cands[i].first, prob, cur));
                ++expansions;
            }
        }
        if (nextBeam.empty()) break;
        // Keep best beamWidth states by (validity, time, peak)
        std::sort(nextBeam.begin(), nextBeam.end(), [&](const ScheduleState& a, const ScheduleState& b){
            bool aValid = a.memory_peak <= prob.total_memory;
            bool bValid = b.memory_peak <= prob.total_memory;
            if (aValid != bValid) return aValid; // valid first
            if (a.total_time != b.total_time) return a.total_time < b.total_time;
            return a.memory_peak < b.memory_peak;
        });
        if (nextBeam.size() > beamWidth) nextBeam.resize(beamWidth);
        beam.swap(nextBeam);
    }
    return has_best ? best : (beam.empty() ? ScheduleState{} : beam.front());
}

// DP+Greedy: limited lookahead search selecting the best frontier by (feasible peak, time)
ScheduleState dpGreedySchedule(const Problem& prob, size_t lookaheadDepth, size_t branchFactor) {
    if (lookaheadDepth == 0) lookaheadDepth = 2; if (branchFactor == 0) branchFactor = 8;
    ScheduleState cur;
    while (cur.computed.size() < prob.nodes.size()) {
        auto ready = getReadyNodeNames(prob, cur);
        if (ready.empty()) break;
        // Score candidates by exploring up to lookaheadDepth with branching
        std::string bestName; int bestPeak = std::numeric_limits<int>::max(); int bestTime = std::numeric_limits<int>::max();
        // Rank current ready by predicted peak/time, take top branchFactor to explore deeper
        std::vector<std::pair<std::string, std::pair<int,int>>> cands;
        for (const auto& nm : ready) {
            const Node& node = prob.nodes.at(nm);
            int p = calculateSequentialPeak(cur, node, cur.current_memory);
            cands.push_back({nm, {p, node.getTimeCost()}});
        }
        std::sort(cands.begin(), cands.end(), [](const auto& a, const auto& b){
            if (a.second.first != b.second.first) return a.second.first < b.second.first;
            return a.second.second < b.second.second;
        });
        size_t explore = std::min(cands.size(), branchFactor);
        auto evalPath = [&](const ScheduleState& start, const std::string& first)->std::pair<int,int>{
            ScheduleState tmp = executeNode(first, prob, start);
            size_t depth = 1;
            while (depth < lookaheadDepth && tmp.computed.size() < prob.nodes.size()) {
                auto r = getReadyNodeNames(prob, tmp);
                if (r.empty()) break;
                // greedy inside lookahead: pick candidate minimizing predicted peak then time
                std::string pick; int bestP = std::numeric_limits<int>::max(); int bestT = std::numeric_limits<int>::max();
                for (const auto& nm : r) {
                    const Node& node = prob.nodes.at(nm);
                    int p = calculateSequentialPeak(tmp, node, tmp.current_memory);
                    int t = node.getTimeCost();
                    if (p < bestP || (p == bestP && t < bestT)) { bestP = p; bestT = t; pick = nm; }
                }
                if (pick.empty()) break;
                tmp = executeNode(pick, prob, tmp);
                ++depth;
            }
            return {tmp.memory_peak, tmp.total_time};
        };
        for (size_t i = 0; i < explore; ++i) {
            auto [p, t] = evalPath(cur, cands[i].first);
            if (p <= prob.total_memory && (p < bestPeak || (p == bestPeak && t < bestTime))) {
                bestPeak = p; bestTime = t; bestName = cands[i].first;
            }
        }
        if (bestName.empty()) {
            // fall back to immediate best by predicted peak
            bestName = cands.front().first;
        }
        cur = executeNode(bestName, prob, cur);
    }
    return cur;
}

ScheduleState scheduleWithDebug(const Problem& prob, size_t maxExpansions, double timeLimitSeconds,
                                const DebugOptions& opts, DebugStats& stats) {
    ScheduleState init; ScheduleState best; bool has_best = false;
    if (maxExpansions == 0) maxExpansions = 100000;
    if (timeLimitSeconds <= 0.0) timeLimitSeconds = 2.0;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
        std::chrono::duration<double>(timeLimitSeconds));
    size_t left = maxExpansions;
    dfsScheduleLimited(prob, init, best, has_best, left, deadline, &opts, &stats);
    if (opts.verbose) {
        std::cerr << "dbg: expansions=" << stats.expansions
                  << " prunedByMemory=" << stats.prunedByMemory
                  << " deadEnds=" << stats.deadEnds
                  << " found=" << (has_best ? 1 : 0) << "\n";
    }
    return has_best ? best : ScheduleState{};
}

// Hybrid MILP Scheduler using Gurobi with greedy warm start
ScheduleState hybridMilpSchedule(const Problem& prob, double timeLimitSeconds, bool useWarmStart) {
    try {
        
        // Create Gurobi environment and model
        GRBEnv env = GRBEnv(true);
        env.start();
        GRBModel model = GRBModel(env);
        
        // Set time limit
        model.set(GRB_DoubleParam_TimeLimit, timeLimitSeconds);
        
        // Variables: x[i][t] = 1 if node i is executed at time t
        std::map<std::string, std::vector<GRBVar>> nodeVars;
        std::vector<std::string> nodeNames;
        for (const auto& kv : prob.nodes) {
            nodeNames.push_back(kv.first);
        }
        
        int maxTime = nodeNames.size(); // Upper bound on time steps
        
        // Create binary variables x[i][t]
        for (const auto& nodeName : nodeNames) {
            nodeVars[nodeName].reserve(maxTime);
            for (int t = 0; t < maxTime; t++) {
                nodeVars[nodeName].push_back(
                    model.addVar(0.0, 1.0, 0.0, GRB_BINARY, 
                                "x_" + nodeName + "_" + std::to_string(t))
                );
            }
        }
        
        // Memory variables: m[t] = memory usage at time t
        std::vector<GRBVar> memoryVars;
        memoryVars.reserve(maxTime + 1);
        for (int t = 0; t <= maxTime; t++) {
            memoryVars.push_back(
                model.addVar(0.0, GRB_INFINITY, 0.0, GRB_CONTINUOUS, 
                            "m_" + std::to_string(t))
            );
        }
        
        // Makespan variable
        GRBVar makespan = model.addVar(0.0, GRB_INFINITY, 1.0, GRB_CONTINUOUS, "makespan");
        
        model.update();
        
        // Constraints
        
        // 1. Each node executed exactly once
        for (const auto& nodeName : nodeNames) {
            GRBLinExpr sum = 0;
            for (int t = 0; t < maxTime; t++) {
                sum += nodeVars[nodeName][t];
            }
            model.addConstr(sum == 1, "once_" + nodeName);
        }
        
        // 2. Precedence constraints
        for (const auto& kv : prob.nodes) {
            const std::string& nodeName = kv.first;
            const Node& node = kv.second;
            
            for (const auto& inputName : node.getInputs()) {
                if (prob.nodes.find(inputName) != prob.nodes.end()) {
                    // Input must finish before this node starts
                    for (int t = 0; t < maxTime; t++) {
                        GRBLinExpr predecessorSum = 0;
                        for (int tau = 0; tau <= t; tau++) {
                            if (tau < maxTime) {
                                predecessorSum += nodeVars[inputName][tau];
                            }
                        }
                        model.addConstr(predecessorSum >= nodeVars[nodeName][t], 
                                      "prec_" + inputName + "_" + nodeName + "_" + std::to_string(t));
                    }
                }
            }
        }
        
        // 3. Memory constraints (simplified)
        for (int t = 0; t < maxTime; t++) {
            GRBLinExpr memUsage = 0;
            
            for (const auto& kv : prob.nodes) {
                const std::string& nodeName = kv.first;
                const Node& node = kv.second;
                
                // Add memory for outputs of nodes executed up to time t
                GRBLinExpr nodeExecuted = 0;
                for (int tau = 0; tau <= t && tau < maxTime; tau++) {
                    nodeExecuted += nodeVars[nodeName][tau];
                }
                
                memUsage += nodeExecuted * node.getPeak();
            }
            
            model.addConstr(memoryVars[t] >= memUsage, "mem_" + std::to_string(t));
            model.addConstr(memoryVars[t] <= prob.total_memory, "mem_limit_" + std::to_string(t));
        }
        
        // 4. Makespan constraints
        for (const auto& nodeName : nodeNames) {
            const Node& node = prob.nodes.at(nodeName);
            for (int t = 0; t < maxTime; t++) {
                model.addConstr(makespan >= (t + node.getTimeCost()) * nodeVars[nodeName][t], 
                              "makespan_" + nodeName + "_" + std::to_string(t));
            }
        }
        
        // Objective: minimize makespan (with small memory penalty)
        GRBLinExpr totalMem = 0;
        for (int t = 0; t <= maxTime; t++) {
            totalMem += memoryVars[t];
        }
        model.setObjective(makespan + 0.001 * totalMem, GRB_MINIMIZE);
        
        // Warm start with greedy solution if requested
        if (useWarmStart) {
            ScheduleState greedySol = greedySchedule(prob);
            if (!greedySol.execution_order.empty()) {
                std::cout << "Using greedy warm start with " << greedySol.execution_order.size() << " operations\n";
                
                // Set warm start values
                for (int i = 0; i < (int)greedySol.execution_order.size() && i < maxTime; i++) {
                    const std::string& nodeName = greedySol.execution_order[i];
                    if (nodeVars.find(nodeName) != nodeVars.end()) {
                        for (int t = 0; t < maxTime; t++) {
                            nodeVars[nodeName][t].set(GRB_DoubleAttr_Start, (t == i) ? 1.0 : 0.0);
                        }
                    }
                }
            }
        }
        
        // Optimize
        model.optimize();
        
        // Extract solution
        ScheduleState result;
        
        if (model.get(GRB_IntAttr_Status) == GRB_OPTIMAL || 
            model.get(GRB_IntAttr_Status) == GRB_TIME_LIMIT) {
            
            std::vector<std::pair<int, std::string>> schedule;
            
            for (const auto& nodeName : nodeNames) {
                for (int t = 0; t < maxTime; t++) {
                    if (nodeVars[nodeName][t].get(GRB_DoubleAttr_X) > 0.5) {
                        schedule.push_back({t, nodeName});
                        break;
                    }
                }
            }
            
            // Sort by execution time
            std::sort(schedule.begin(), schedule.end());
            
            // Build result
            for (const auto& item : schedule) {
                result.execution_order.push_back(item.second);
            }
            
            // Calculate metrics (simplified)
            result.total_time = (long)model.get(GRB_DoubleAttr_ObjVal);
            result.memory_peak = 0;
            
            // Simulate execution to get accurate memory peak
            ScheduleState temp;
            for (const auto& nodeName : result.execution_order) {
                if (prob.nodes.find(nodeName) != prob.nodes.end()) {
                    const Node& node = prob.nodes.at(nodeName);
                    temp.memory_peak = std::max((long)temp.memory_peak, (long)(temp.memory_peak + node.getPeak()));
                    temp.computed.insert(nodeName);
                }
            }
            result.memory_peak = temp.memory_peak;
            
            std::cout << "MILP solution: makespan = " << result.total_time 
                     << ", memory peak = " << result.memory_peak << std::endl;
        } else {
            std::cout << "MILP failed to find solution, falling back to greedy" << std::endl;
            result = greedySchedule(prob);
        }
        
        return result;
        
    } catch (const std::exception& e) {
        std::cerr << "MILP error: " << e.what() << std::endl;
        std::cout << "Falling back to greedy schedule" << std::endl;
        return greedySchedule(prob);
    }
}

// Multi-Stage Hybrid Scheduler
ScheduleState hybridMultiStageSchedule(const Problem& prob, double timeLimitSeconds) {
    std::cout << "=== Multi-Stage Hybrid Scheduler ===" << std::endl;
    
    // Stage 1: Heuristic/Metaheuristic Stage
    std::cout << "Stage 1: Heuristic exploration..." << std::endl;
    
    std::vector<ScheduleState> candidates;
    std::vector<std::string> heuristicNames;
    
    // Run multiple heuristics to get diverse solutions
    auto start_time = std::chrono::steady_clock::now();
    double stage1_time = timeLimitSeconds * 0.3; // 30% of time for heuristics
    
    // 1.1 Greedy heuristic
    auto greedy_result = greedySchedule(prob);
    if (!greedy_result.execution_order.empty()) {
        candidates.push_back(greedy_result);
        heuristicNames.push_back("Greedy");
    }
    
    // 1.2 Beam search with different widths
    for (size_t width : {8, 16, 32}) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - start_time).count();
        if (elapsed > stage1_time) break;
        
        auto beam_result = beamSearchSchedule(prob, width, 50000);
        if (!beam_result.execution_order.empty()) {
            candidates.push_back(beam_result);
            heuristicNames.push_back("Beam-" + std::to_string(width));
        }
    }
    
    // 1.3 DP-Greedy with different parameters
    for (size_t depth : {2, 4}) {
        for (size_t branch : {4, 8}) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - start_time).count();
            if (elapsed > stage1_time) break;
            
            auto dp_result = dpGreedySchedule(prob, depth, branch);
            if (!dp_result.execution_order.empty()) {
                candidates.push_back(dp_result);
                heuristicNames.push_back("DP-" + std::to_string(depth) + "-" + std::to_string(branch));
            }
        }
    }
    
    std::cout << "Found " << candidates.size() << " heuristic solutions" << std::endl;
    
    if (candidates.empty()) {
        std::cout << "No heuristic solutions found, cannot proceed to MILP stage" << std::endl;
        return ScheduleState{};
    }
    
    // Select best heuristic solutions
    std::sort(candidates.begin(), candidates.end(), [&](const ScheduleState& a, const ScheduleState& b) {
        return isBetterSchedule(a, b, prob.total_memory);
    });
    
    // Keep top 3 solutions for analysis
    size_t num_candidates = std::min((size_t)3, candidates.size());
    candidates.resize(num_candidates);
    
    std::cout << "Top heuristic solutions:" << std::endl;
    for (size_t i = 0; i < candidates.size(); i++) {
        bool feasible = candidates[i].memory_peak <= prob.total_memory;
        std::cout << "  " << heuristicNames[i] << ": time=" << candidates[i].total_time 
                  << ", memory=" << candidates[i].memory_peak 
                  << "/" << prob.total_memory << (feasible ? " (feasible)" : " (infeasible)") << std::endl;
    }
    
    // Stage 2: MILP Refinement Stage
    std::cout << "Stage 2: MILP refinement..." << std::endl;
    
    double stage2_time = timeLimitSeconds * 0.6; // 60% of time for MILP
    ScheduleState best_solution = candidates[0];
    
    try {
        // Create Gurobi environment and model
        GRBEnv env = GRBEnv(true);
        env.start();
        GRBModel model = GRBModel(env);
        
        // Set time limit for MILP stage
        model.set(GRB_DoubleParam_TimeLimit, stage2_time);
        
        // Analyze heuristic solutions to identify "hard core" variables
        std::set<std::string> critical_nodes;
        std::map<std::string, std::set<int>> possible_positions;
        
        // Find nodes that appear in different positions across solutions
        for (const auto& solution : candidates) {
            for (size_t pos = 0; pos < solution.execution_order.size(); pos++) {
                const std::string& node = solution.execution_order[pos];
                possible_positions[node].insert(pos);
                
                // Mark as critical if it appears in different positions
                if (possible_positions[node].size() > 1) {
                    critical_nodes.insert(node);
                }
            }
        }
        
        std::cout << "Identified " << critical_nodes.size() << " critical nodes for MILP optimization" << std::endl;
        
        // Create focused MILP model
        std::vector<std::string> nodeNames;
        for (const auto& kv : prob.nodes) {
            nodeNames.push_back(kv.first);
        }
        
        int maxTime = nodeNames.size();
        std::map<std::string, std::vector<GRBVar>> nodeVars;
        
        // Create variables only for critical nodes or flexible positions
        for (const auto& nodeName : nodeNames) {
            nodeVars[nodeName].reserve(maxTime);
            for (int t = 0; t < maxTime; t++) {
                if (critical_nodes.count(nodeName) || 
                    possible_positions[nodeName].count(t)) {
                    nodeVars[nodeName].push_back(
                        model.addVar(0.0, 1.0, 0.0, GRB_BINARY, 
                                    "x_" + nodeName + "_" + std::to_string(t))
                    );
                } else {
                    // Fix variable based on heuristic solution
                    bool should_execute = false;
                    if (!candidates[0].execution_order.empty() && 
                        t < (int)candidates[0].execution_order.size() && 
                        candidates[0].execution_order[t] == nodeName) {
                        should_execute = true;
                    }
                    nodeVars[nodeName].push_back(
                        model.addVar(should_execute ? 1.0 : 0.0, should_execute ? 1.0 : 0.0, 
                                    0.0, GRB_BINARY, 
                                    "x_" + nodeName + "_" + std::to_string(t))
                    );
                }
            }
        }
        
        // Makespan variable
        GRBVar makespan = model.addVar(0.0, GRB_INFINITY, 1.0, GRB_CONTINUOUS, "makespan");
        
        model.update();
        
        // Add constraints (simplified for critical nodes)
        
        // Each node executed exactly once
        for (const auto& nodeName : nodeNames) {
            GRBLinExpr sum = 0;
            for (int t = 0; t < maxTime; t++) {
                sum += nodeVars[nodeName][t];
            }
            model.addConstr(sum == 1, "once_" + nodeName);
        }
        
        // Precedence constraints
        for (const auto& kv : prob.nodes) {
            const std::string& nodeName = kv.first;
            const Node& node = kv.second;
            
            for (const auto& inputName : node.getInputs()) {
                if (prob.nodes.find(inputName) != prob.nodes.end()) {
                    for (int t = 0; t < maxTime; t++) {
                        GRBLinExpr predecessorSum = 0;
                        for (int tau = 0; tau <= t; tau++) {
                            if (tau < maxTime) {
                                predecessorSum += nodeVars[inputName][tau];
                            }
                        }
                        model.addConstr(predecessorSum >= nodeVars[nodeName][t], 
                                      "prec_" + inputName + "_" + nodeName + "_" + std::to_string(t));
                    }
                }
            }
        }
        
        // Simplified memory constraints
        for (int t = 0; t < maxTime; t++) {
            GRBLinExpr memUsage = 0;
            
            for (const auto& kv : prob.nodes) {
                const std::string& nodeName = kv.first;
                const Node& node = kv.second;
                
                GRBLinExpr nodeExecuted = 0;
                for (int tau = 0; tau <= t && tau < maxTime; tau++) {
                    nodeExecuted += nodeVars[nodeName][tau];
                }
                
                memUsage += nodeExecuted * node.getPeak();
            }
            
            model.addConstr(memUsage <= prob.total_memory, "mem_limit_" + std::to_string(t));
        }
        
        // Makespan constraints
        for (const auto& nodeName : nodeNames) {
            const Node& node = prob.nodes.at(nodeName);
            for (int t = 0; t < maxTime; t++) {
                model.addConstr(makespan >= (t + node.getTimeCost()) * nodeVars[nodeName][t], 
                              "makespan_" + nodeName + "_" + std::to_string(t));
            }
        }
        
        // Objective: minimize makespan
        GRBLinExpr objective = makespan;
        model.setObjective(objective, GRB_MINIMIZE);
        
        // Set warm start from best heuristic solution
        for (int i = 0; i < (int)candidates[0].execution_order.size() && i < maxTime; i++) {
            const std::string& nodeName = candidates[0].execution_order[i];
            if (nodeVars.find(nodeName) != nodeVars.end()) {
                for (int t = 0; t < maxTime; t++) {
                    nodeVars[nodeName][t].set(GRB_DoubleAttr_Start, (t == i) ? 1.0 : 0.0);
                }
            }
        }
        
        std::cout << "Optimizing focused MILP model..." << std::endl;
        model.optimize();
        
        // Extract MILP solution
        if (model.get(GRB_IntAttr_Status) == GRB_OPTIMAL || 
            model.get(GRB_IntAttr_Status) == GRB_TIME_LIMIT) {
            
            std::vector<std::pair<int, std::string>> schedule;
            
            for (const auto& nodeName : nodeNames) {
                for (int t = 0; t < maxTime; t++) {
                    if (nodeVars[nodeName][t].get(GRB_DoubleAttr_X) > 0.5) {
                        schedule.push_back({t, nodeName});
                        break;
                    }
                }
            }
            
            std::sort(schedule.begin(), schedule.end());
            
            ScheduleState milp_result;
            for (const auto& item : schedule) {
                milp_result.execution_order.push_back(item.second);
            }
            milp_result.total_time = (long)model.get(GRB_DoubleAttr_ObjVal);
            
            std::cout << "MILP optimization completed: makespan = " << milp_result.total_time << std::endl;
            best_solution = milp_result;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "MILP refinement failed: " << e.what() << std::endl;
        std::cout << "Using best heuristic solution" << std::endl;
    }
    
    // Stage 3: Validation/Simulation Stage
    std::cout << "Stage 3: Validation and simulation..." << std::endl;
    
    // Simulate the schedule to get accurate memory usage
    ScheduleState validated_result;
    validated_result.execution_order = best_solution.execution_order;
    
    int current_memory = 0;
    int memory_peak = 0;
    int total_time = 0;
    std::unordered_set<std::string> computed;
    std::unordered_map<std::string, int> output_memory;
    
    for (const auto& nodeName : validated_result.execution_order) {
        if (prob.nodes.find(nodeName) == prob.nodes.end()) continue;
        
        const Node& node = prob.nodes.at(nodeName);
        
        // Add this node's memory requirements
        current_memory += node.getPeak();
        memory_peak = std::max(memory_peak, current_memory);
        total_time += node.getTimeCost();
        
        // Track outputs
        output_memory[nodeName] = node.getOutputMem();
        computed.insert(nodeName);
        
        // Free inputs that are no longer needed
        for (const auto& inputName : node.getInputs()) {
            if (output_memory.count(inputName)) {
                bool canFree = true;
                // Check if any uncomputed nodes still need this input
                for (const auto& kv : prob.nodes) {
                    if (!computed.count(kv.first)) {
                        for (const auto& inp : kv.second.getInputs()) {
                            if (inp == inputName) {
                                canFree = false;
                                break;
                            }
                        }
                        if (!canFree) break;
                    }
                }
                
                if (canFree) {
                    current_memory -= output_memory[inputName];
                    output_memory.erase(inputName);
                }
            }
        }
    }
    
    validated_result.memory_peak = memory_peak;
    validated_result.total_time = total_time;
    validated_result.current_memory = current_memory;
    validated_result.computed = computed;
    validated_result.output_memory = output_memory;
    
    // Validation check
    bool is_feasible = (validated_result.memory_peak <= prob.total_memory);
    bool is_complete = (validated_result.execution_order.size() == prob.nodes.size());
    
    std::cout << "Validation results:" << std::endl;
    std::cout << "  Complete: " << (is_complete ? "Yes" : "No") << std::endl;
    std::cout << "  Feasible: " << (is_feasible ? "Yes" : "No") << std::endl;
    std::cout << "  Total time: " << validated_result.total_time << std::endl;
    std::cout << "  Memory peak: " << validated_result.memory_peak 
              << "/" << prob.total_memory << std::endl;
    
    if (!is_feasible && candidates.size() > 1) {
        std::cout << "Primary solution infeasible, trying backup heuristic..." << std::endl;
        return candidates[1]; // Return second-best heuristic solution
    }
    
    return validated_result;
}


