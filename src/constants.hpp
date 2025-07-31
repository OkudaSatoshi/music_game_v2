#pragma once

#include <SFML/Graphics.hpp>
#include <vector>

// --- 定数定義 ---
const int WINDOW_WIDTH = 1920;
const int WINDOW_HEIGHT = 1080;
const int LANE_COUNT = 6;
const float LANE_WIDTH = 80.f; // 50から変更
const float NOTE_HEIGHT = 30.f; // 20から変更
const float LANE_AREA_WIDTH = LANE_COUNT * LANE_WIDTH;
const float LANE_START_X = (WINDOW_WIDTH - LANE_AREA_WIDTH) / 2.f;
const float JUDGMENT_LINE_Y = WINDOW_HEIGHT - 100.f;
const float NOTE_PIXELS_PER_SECOND = 350.f; // ノーツの落下速度 (ピクセル/秒)

// --- 判定範囲 (小さいほど厳しい) ---
const float PERFECT_WINDOW = 0.08f; // 秒 (±80ms)
const float GREAT_WINDOW = 0.15f;  // 秒 (±150ms)

// --- 色の定義 ---
const sf::Color LANE_COLOR_NORMAL = sf::Color(50, 50, 50, 128);
const sf::Color LANE_COLOR_PRESSED = sf::Color(255, 255, 0, 180);

// --- キーのマッピング ---
const std::vector<sf::Keyboard::Key> LANE_KEYS = {
    sf::Keyboard::S, sf::Keyboard::D, sf::Keyboard::F,
    sf::Keyboard::J, sf::Keyboard::K, sf::Keyboard::L
};
