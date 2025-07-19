#pragma once

#include <SFML/Graphics.hpp>
#include <string>
#include <vector>

// --- ゲームの状態 ---
enum class GameState {
    TITLE,
    OPTIONS,
    SONG_SELECTION,
    DIFFICULTY_SELECTION,
    PLAYING,
    PAUSED,
    GAMEOVER,
    RESULTS
};

// --- 判定結果のenum ---
enum class Judgment {
    NONE,
    PERFECT,
    GREAT,
    MISS
};

// --- データ構造 ---
struct Note
{
    sf::RectangleShape shape;
    int laneIndex;
    double spawnTime; // ノーツが判定ラインに到達すべき時間 (秒)
    bool isProcessed = false; // 判定済みかどうかのフラグ
};

struct ChartData
{
    std::string difficultyName;
    std::string chartPath;
};

struct SongData
{
    std::string title;
    std::string audioPath;
    std::string backgroundPath; // 背景画像パスを追加
    std::vector<ChartData> charts;
};

struct Particle {
    sf::CircleShape shape;
    sf::Vector2f velocity;
    sf::Time lifetime;
};

struct GameConfig {
    float noteSpeedMultiplier = 1.0f;
    float bgmVolume = 100.0f;
    float sfxVolume = 100.0f;
    float audioOffset = 0.0f; // ms
};
