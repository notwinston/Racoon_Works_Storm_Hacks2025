#pragma once

#include "model.hpp"
/**
 * calculateSequentialPeak function - Calculates the sequential peak
 * 
 * Attributes:
 * - state: The schedule state
 * - node_B: The node B
 * - impact_A: The impact A
 */
int calculateSequentialPeak(const ScheduleState& state, const Node& node_B, int impact_A);
bool isBetterSchedule(const ScheduleState& state1, const ScheduleState& state2, long total_memory);
std::unordered_set<std::string> getFreeableInputs(
    const Node& node,
    const ScheduleState& state,
    const std::unordered_map<std::string, std::unordered_set<std::string>>& dependencies);

ScheduleState schedule(const Problem& prob);
ScheduleState scheduleWithLimits(const Problem& prob, size_t maxExpansions, double timeLimitSeconds);
ScheduleState greedySchedule(const Problem& prob);


