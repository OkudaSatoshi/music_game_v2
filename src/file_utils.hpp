#pragma once

#include <string>
#include <vector>
#include <map>
#include "types.hpp"

// for convenience
#include "json.hpp"
using json = nlohmann::json;

// --- 関数宣言 ---

// ハイスコア関連
std::string generateHighScoreKey(const SongData& song, const ChartData& chart);
std::map<std::string, int> loadHighScores();
void saveHighScores(const std::map<std::string, int>& highScores);

// 設定関連
GameConfig loadConfig();
void saveConfig(const GameConfig& config);

// 譜面読み込み
std::vector<Note> loadChartFromMidi(const std::string& path);
