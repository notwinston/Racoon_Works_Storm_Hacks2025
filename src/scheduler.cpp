#include "scheduler.hpp"
#include <chrono>
#include <iostream>

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
        if (state.computed.count(name)) continue;
        const Node& n = kv.second;
        bool ok = true;
        for (const auto& inName : n.getInputs()) {
            if (state.computed.count(inName) == 0) { ok = false; break; }
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

static void dfsSchedule(const Problem& prob, ScheduleState& current, ScheduleState& best, bool& has_best) {
    if (current.computed.size() == prob.nodes.size()) {
        if (!has_best || isBetterSchedule(current, best, prob.total_memory)) { best = current; has_best = true; }
        return;
    }
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
    if (ready.empty()) { if (stats) stats->deadEnds++; return; }
    ready = pruneReadyListDynamic(ready, prob, current);
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


