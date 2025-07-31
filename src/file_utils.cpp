#include "file_utils.hpp"
#include "constants.hpp"
#include <fstream>
#include <iomanip>
#include "MidiFile.h"
#include <algorithm>

// --- ハイスコア関連のヘルパー関数 ---

// 曲と難易度からJSONのキーを生成する
std::string generateHighScoreKey(const SongData& song, const ChartData& chart) {
    return song.title + "-" + chart.difficultyName;
}

// scores.json からハイスコアを読み込む
std::map<std::string, int> loadHighScores() {
    std::map<std::string, int> highScores;
    std::ifstream ifs("scores.json");
    if (ifs.is_open() && ifs.peek() != std::ifstream::traits_type::eof()) {
        try {
            json scoresJson = json::parse(ifs);
            for (auto it = scoresJson.begin(); it != scoresJson.end(); ++it) {
                highScores[it.key()] = it.value();
            }
        } catch (const json::parse_error& e) {
            // パースエラーが起きても、空のスコアでゲームを続行
        }
    }
    return highScores;
}

// scores.json にハイスコアを保存する
void saveHighScores(const std::map<std::string, int>& highScores) {
    json scoresJson;
    for (const auto& pair : highScores) {
        scoresJson[pair.first] = pair.second;
    }
    std::ofstream ofs("scores.json");
    ofs << std::setw(4) << scoresJson << std::endl;
}

// --- 設定関連のヘルパー関数 ---

// config.json から設定を読み込む
GameConfig loadConfig() {
    GameConfig config;
    std::ifstream ifs("config.json");
    if (ifs.is_open() && ifs.peek() != std::ifstream::traits_type::eof()) {
        try {
            json configJson = json::parse(ifs);
            if (configJson.contains("note_speed_multiplier")) {
                config.noteSpeedMultiplier = configJson["note_speed_multiplier"].get<float>();
            }
            if (configJson.contains("bgm_volume")) {
                config.bgmVolume = configJson["bgm_volume"].get<float>();
            }
            if (configJson.contains("sfx_volume")) {
                config.sfxVolume = configJson["sfx_volume"].get<float>();
            }
            if (configJson.contains("audio_offset")) {
                config.audioOffset = configJson["audio_offset"].get<float>();
            }
        } catch (const json::parse_error& e) {
            // パースエラーが起きても、デフォルト設定でゲームを続行
        }
    }
    return config;
}

// config.json に設定を保存する
void saveConfig(const GameConfig& config) {
    json configJson;
    configJson["note_speed_multiplier"] = config.noteSpeedMultiplier;
    configJson["bgm_volume"] = config.bgmVolume;
    configJson["sfx_volume"] = config.sfxVolume;
    configJson["audio_offset"] = config.audioOffset;
    std::ofstream ofs("config.json");
    ofs << std::setw(4) << configJson << std::endl;
}


// --- 譜面読み込み関数 ---
std::vector<Note> loadChartFromMidi(const std::string& path) {
    smf::MidiFile midiFile;
    if (!midiFile.read(path)) {
        return {}; // 読み込み失敗
    }

    midiFile.doTimeAnalysis();

    std::vector<Note> chart;
    for (int track = 0; track < midiFile.getTrackCount(); ++track) {
        for (int event = 0; event < midiFile[track].size(); ++event) {
            if (midiFile[track][event].isNoteOn()) {
                Note newNote;
                newNote.spawnTime = midiFile[track][event].seconds;
                newNote.laneIndex = midiFile[track][event].getKeyNumber() % LANE_COUNT;
                
                newNote.shape.setSize(sf::Vector2f(LANE_WIDTH, NOTE_HEIGHT));
                newNote.shape.setFillColor(sf::Color::Cyan);
                newNote.shape.setPosition(LANE_START_X + newNote.laneIndex * LANE_WIDTH, -100.f);

                chart.push_back(newNote);
            }
        }
    }
    
    std::sort(chart.begin(), chart.end(), [](const Note& a, const Note& b) {
        return a.spawnTime < b.spawnTime;
    });

    return chart;
}
