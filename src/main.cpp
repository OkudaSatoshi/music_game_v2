#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <vector>
#include <string>
#include <cmath> // for abs()
#include <algorithm> // for sort()
#include <fstream> // for file reading
#include <sstream>   // for string stream
#include <map>       // for high scores
#include <iomanip>   // for json pretty printing
#include "MidiFile.h"
#include "json.hpp"

// for convenience
using json = nlohmann::json;

// --- 定数定義 ---
const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 600;
const int LANE_COUNT = 6;
const float LANE_WIDTH = 50.f;
const float NOTE_HEIGHT = 20.f;
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

// --- ゲームの状態 ---
enum class GameState {
    TITLE,
    OPTIONS,
    SONG_SELECTION,
    DIFFICULTY_SELECTION,
    PLAYING,
    PAUSED,
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
    std::vector<ChartData> charts;
};

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

struct GameConfig {
    float noteSpeedMultiplier = 1.0f;
};

// config.json から設定を読み込む
GameConfig loadConfig() {
    GameConfig config;
    std::ifstream ifs("config.json");
    if (ifs.is_open() && ifs.peek() != std::ifstream::traits_type::eof()) {
        try {
            json configJson = json::parse(ifs);
            if (configJson.contains("note_speed_multiplier")) {
                config.noteSpeedMultiplier = configJson["note_speed_multiplier"];
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


int main()
{
    sf::RenderWindow window(sf::VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT), "Sound Game");
    window.setFramerateLimit(120);

    // --- リソースの事前読み込み ---
    sf::Font font;
    if (!font.loadFromFile("Kazesawa-ExtraLight.ttf")) { return -1; }
    sf::Font scoreFont;
    if (!scoreFont.loadFromFile("Evogria.otf")) { return -1; }

    sf::Texture backgroundTexture;
    if (!backgroundTexture.loadFromFile("img/nasturtium.jpg")) { return -1; }
    sf::Sprite backgroundSprite;
    backgroundSprite.setTexture(backgroundTexture);

    sf::Texture resultBackgroundTexture;
    if (!resultBackgroundTexture.loadFromFile("img/result_bg.jpg")) { return -1; }
    sf::Sprite resultBackgroundSprite;
    resultBackgroundSprite.setTexture(resultBackgroundTexture);

    sf::SoundBuffer tapSoundBuffer;
    if (!tapSoundBuffer.loadFromFile("audio/tap.wav")) { return -1; }
    sf::Sound tapSound;
    tapSound.setBuffer(tapSoundBuffer);

    // --- 曲リストをJSONから読み込み ---
    std::vector<SongData> songs;
    std::ifstream ifs("songs.json");
    if (ifs.is_open())
    {
        json j = json::parse(ifs);
        for (const auto& song_json : j)
        {
            SongData song_data;
            song_data.title = song_json.at("title").get<std::string>();
            song_data.audioPath = song_json.at("audio_path").get<std::string>();
            for (const auto& chart_json : song_json.at("charts"))
            {
                ChartData chart_data;
                chart_data.difficultyName = chart_json.at("difficulty").get<std::string>();
                chart_data.chartPath = chart_json.at("chart_path").get<std::string>();
                song_data.charts.push_back(chart_data);
            }
            songs.push_back(song_data);
        }
    }

    if (songs.empty())
    {
        // JSONが読めなかったか空だった場合のエラー処理
        return -1;
    }

    // --- ハイスコアをJSONから読み込み ---
    auto highScores = loadHighScores();

    // --- 設定をJSONから読み込み ---
    auto config = loadConfig();

    // --- UI要素の準備 ---
    // タイトル画面
    sf::Text titleText("Sound Game", font, 80);
    sf::FloatRect textRect = titleText.getLocalBounds();
    titleText.setOrigin(textRect.left + textRect.width / 2.0f, textRect.top + textRect.height / 2.0f);
    titleText.setPosition(WINDOW_WIDTH / 2.0f, 200.f);

    std::vector<sf::Text> titleMenuTexts(2);
    std::vector<std::string> titleMenuStrings = {"Start Game", "Options"};
    for(size_t i = 0; i < titleMenuTexts.size(); ++i) {
        titleMenuTexts[i].setFont(font);
        titleMenuTexts[i].setCharacterSize(32);
        titleMenuTexts[i].setString(titleMenuStrings[i]);
        textRect = titleMenuTexts[i].getLocalBounds();
        titleMenuTexts[i].setOrigin(textRect.left + textRect.width / 2.0f, textRect.top + textRect.height / 2.0f);
        titleMenuTexts[i].setPosition(WINDOW_WIDTH / 2.0f, 400.f + i * 60.f);
    }

    // 曲選択画面
    sf::Text songSelectionTitle("Select a Song", font, 50);
    textRect = songSelectionTitle.getLocalBounds();
    songSelectionTitle.setOrigin(textRect.left + textRect.width / 2.0f, textRect.top + textRect.height / 2.0f);
    songSelectionTitle.setPosition(WINDOW_WIDTH / 2.0f, 80.f);
    std::vector<sf::Text> songTitleTexts(songs.size());
    for(size_t i = 0; i < songs.size(); ++i) {
        songTitleTexts[i].setFont(font);
        songTitleTexts[i].setCharacterSize(32);
        songTitleTexts[i].setString(songs[i].title);
        textRect = songTitleTexts[i].getLocalBounds();
        songTitleTexts[i].setOrigin(textRect.left + textRect.width / 2.0f, textRect.top + textRect.height / 2.0f);
        songTitleTexts[i].setPosition(WINDOW_WIDTH / 2.0f, 200.f + i * 60.f);
    }

    // 難易度選択画面
    sf::Text difficultySelectionTitle("", font, 50);
    std::vector<sf::Text> difficultyTexts;
    sf::Text difficultyHighScoreText("", scoreFont, 28);
    difficultyHighScoreText.setFillColor(sf::Color(255, 255, 100)); // Light Yellow

    // オプション画面
    sf::Text optionsTitle("Options", font, 60);
    textRect = optionsTitle.getLocalBounds();
    optionsTitle.setOrigin(textRect.left + textRect.width / 2.0f, textRect.top + textRect.height / 2.0f);
    optionsTitle.setPosition(WINDOW_WIDTH / 2.0f, 150.f);

    sf::Text speedOptionText("Note Speed", font, 32);
    speedOptionText.setPosition(WINDOW_WIDTH / 2.0f - 200.f, 300.f);

    sf::Text speedValueText("", font, 32);
    speedValueText.setFillColor(sf::Color::Yellow);

    sf::Text optionsHelpText("Left/Right to change, Enter to save and exit", font, 24);
    textRect = optionsHelpText.getLocalBounds();
    optionsHelpText.setOrigin(textRect.left + textRect.width / 2.0f, textRect.top + textRect.height / 2.0f);
    optionsHelpText.setPosition(WINDOW_WIDTH / 2.0f, WINDOW_HEIGHT - 100.f);

    // ポーズ画面
    sf::RectangleShape pauseOverlay(sf::Vector2f(WINDOW_WIDTH, WINDOW_HEIGHT));
    pauseOverlay.setFillColor(sf::Color(0, 0, 0, 150)); // 半透明の黒
    sf::Text pauseTitle("PAUSED", font, 60);
    textRect = pauseTitle.getLocalBounds();
    pauseTitle.setOrigin(textRect.left + textRect.width / 2.0f, textRect.top + textRect.height / 2.0f);
    pauseTitle.setPosition(WINDOW_WIDTH / 2.0f, 150.f);

    std::vector<sf::Text> pauseMenuTexts(3);
    std::vector<std::string> pauseMenuStrings = {"Resume", "Retry", "Back to Select"};
    for(size_t i = 0; i < pauseMenuTexts.size(); ++i) {
        pauseMenuTexts[i].setFont(font);
        pauseMenuTexts[i].setCharacterSize(32);
        pauseMenuTexts[i].setString(pauseMenuStrings[i]);
        textRect = pauseMenuTexts[i].getLocalBounds();
        pauseMenuTexts[i].setOrigin(textRect.left + textRect.width / 2.0f, textRect.top + textRect.height / 2.0f);
        pauseMenuTexts[i].setPosition(WINDOW_WIDTH / 2.0f, 280.f + i * 60.f);
    }

    // ゲームプレイ画面
    sf::Text scoreText("", scoreFont, 30);
    scoreText.setPosition(10, 10);
    scoreText.setOutlineColor(sf::Color::Black);
    scoreText.setOutlineThickness(2.f);

    sf::Text comboText("", font, 48);
    sf::Text judgmentText("", font, 36);
    sf::Clock judgmentClock;

    // リザルト画面
    sf::Text resultsTitle("Results", scoreFont, 60);
    resultsTitle.setOutlineColor(sf::Color::Black);
    resultsTitle.setOutlineThickness(2.f);
    textRect = resultsTitle.getLocalBounds();
    resultsTitle.setOrigin(textRect.left + textRect.width / 2.0f, textRect.top + textRect.height / 2.0f);
    resultsTitle.setPosition(WINDOW_WIDTH / 2.0f, 100.f);

    sf::Text finalScoreText("", scoreFont, 40);
    finalScoreText.setOutlineColor(sf::Color::Black);
    finalScoreText.setOutlineThickness(2.f);

    sf::Text maxComboText("", scoreFont, 40);
    maxComboText.setOutlineColor(sf::Color::Black);
    maxComboText.setOutlineThickness(2.f);

    sf::Text perfectCountText("", scoreFont, 32);
    perfectCountText.setOutlineColor(sf::Color::Black);
    perfectCountText.setOutlineThickness(2.f);
    perfectCountText.setFillColor(sf::Color::Cyan);

    sf::Text greatCountText("", scoreFont, 32);
    greatCountText.setOutlineColor(sf::Color::Black);
    greatCountText.setOutlineThickness(2.f);
    greatCountText.setFillColor(sf::Color::Yellow);

    sf::Text missCountText("", scoreFont, 32);
    missCountText.setOutlineColor(sf::Color::Black);
    missCountText.setOutlineThickness(2.f);
    missCountText.setFillColor(sf::Color::Red);

    sf::Text newRecordText("", scoreFont, 40);
    newRecordText.setOutlineColor(sf::Color::Black);
    newRecordText.setOutlineThickness(2.f);
    newRecordText.setFillColor(sf::Color::Yellow);

    std::vector<sf::Text> resultsMenuTexts(2);
    std::vector<std::string> resultsMenuStrings = {"Retry", "Back to Select"};
    for(size_t i = 0; i < resultsMenuTexts.size(); ++i) {
        resultsMenuTexts[i].setFont(scoreFont); // フォントを変更
        resultsMenuTexts[i].setCharacterSize(32);
        resultsMenuTexts[i].setString(resultsMenuStrings[i]);
        resultsMenuTexts[i].setOutlineColor(sf::Color::Black);
        resultsMenuTexts[i].setOutlineThickness(2.f);
        textRect = resultsMenuTexts[i].getLocalBounds();
        resultsMenuTexts[i].setOrigin(textRect.left + textRect.width / 2.0f, textRect.top + textRect.height / 2.0f);
        // 横並びのレイアウトに変更
        float xPos = (WINDOW_WIDTH / (resultsMenuTexts.size() + 1.0f)) * (i + 1.0f);
        resultsMenuTexts[i].setPosition(xPos, WINDOW_HEIGHT - 100.f);
    }


    // --- ゲームプレイ用オブジェクト ---
    std::vector<sf::RectangleShape> lanes(LANE_COUNT);
    for (int i = 0; i < LANE_COUNT; ++i) {
        lanes[i].setSize(sf::Vector2f(LANE_WIDTH - 2.f, WINDOW_HEIGHT));
        lanes[i].setPosition(LANE_START_X + i * LANE_WIDTH, 0);
        lanes[i].setOutlineColor(sf::Color::White);
    }
    sf::RectangleShape judgmentLine(sf::Vector2f(LANE_AREA_WIDTH, 2.f));
    judgmentLine.setPosition(LANE_START_X, JUDGMENT_LINE_Y);
    judgmentLine.setFillColor(sf::Color::Red);

    // --- ゲームの状態と変数 ---
    GameState gameState = GameState::TITLE;
    int score = 0;
    int combo = 0;
    int maxCombo = 0;
    int perfectCount = 0;
    int greatCount = 0;
    int missCount = 0;
    size_t nextNoteIndex = 0;
    std::vector<Note> activeNotes;
    std::vector<Note> chart;
    sf::Music music;
    size_t selectedSongIndex = 0;
    size_t selectedDifficultyIndex = 0;
    size_t selectedPauseMenuIndex = 0;
    size_t selectedTitleMenuIndex = 0;
    size_t selectedResultsMenuIndex = 0;

    std::vector<sf::Clock> laneFlashClocks(LANE_COUNT);

    // --- ゲームループ ---
    while (window.isOpen())
    {
        // --- イベント処理 ---
        sf::Event event;
        while (window.pollEvent(event))
        {
            if (event.type == sf::Event::Closed) window.close();

            if (gameState == GameState::TITLE)
            {
                if (event.type == sf::Event::KeyPressed) {
                    if (event.key.code == sf::Keyboard::Down) {
                        selectedTitleMenuIndex = (selectedTitleMenuIndex + 1) % titleMenuTexts.size();
                    } else if (event.key.code == sf::Keyboard::Up) {
                        selectedTitleMenuIndex = (selectedTitleMenuIndex + titleMenuTexts.size() - 1) % titleMenuTexts.size();
                    } else if (event.key.code == sf::Keyboard::Enter) {
                        if (selectedTitleMenuIndex == 0) { // Start Game
                            gameState = GameState::SONG_SELECTION;
                        } else if (selectedTitleMenuIndex == 1) { // Options
                            gameState = GameState::OPTIONS;
                        }
                    }
                }
            }
            else if (gameState == GameState::OPTIONS)
            {
                if (event.type == sf::Event::KeyPressed) {
                    if (event.key.code == sf::Keyboard::Right) {
                        config.noteSpeedMultiplier += 0.1f;
                        if (config.noteSpeedMultiplier > 5.0f) config.noteSpeedMultiplier = 5.0f;
                    } else if (event.key.code == sf::Keyboard::Left) {
                        config.noteSpeedMultiplier -= 0.1f;
                        if (config.noteSpeedMultiplier < 0.1f) config.noteSpeedMultiplier = 0.1f;
                    } else if (event.key.code == sf::Keyboard::Enter || event.key.code == sf::Keyboard::Escape) {
                        saveConfig(config);
                        gameState = GameState::TITLE;
                    }
                }
            }
            else if (gameState == GameState::SONG_SELECTION)
            {
                if (event.type == sf::Event::KeyPressed)
                {
                    if (event.key.code == sf::Keyboard::Down)
                    {
                        selectedSongIndex = (selectedSongIndex + 1) % songs.size();
                    }
                    else if (event.key.code == sf::Keyboard::Up)
                    {
                        selectedSongIndex = (selectedSongIndex + songs.size() - 1) % songs.size();
                    }
                    else if (event.key.code == sf::Keyboard::Enter)
                    {
                        gameState = GameState::DIFFICULTY_SELECTION;
                        selectedDifficultyIndex = 0; // Reset difficulty selection

                        // 難易度選択UIの動的生成
                        const auto& selectedSong = songs[selectedSongIndex];
                        difficultySelectionTitle.setString(selectedSong.title);
                        sf::FloatRect textRect = difficultySelectionTitle.getLocalBounds();
                        difficultySelectionTitle.setOrigin(textRect.left + textRect.width / 2.0f, textRect.top + textRect.height / 2.0f);
                        difficultySelectionTitle.setPosition(WINDOW_WIDTH / 2.0f, 80.f);

                        difficultyTexts.clear();
                        for(size_t i = 0; i < selectedSong.charts.size(); ++i) {
                            sf::Text diffText;
                            diffText.setFont(font);
                            diffText.setCharacterSize(32);
                            diffText.setString(selectedSong.charts[i].difficultyName);
                            textRect = diffText.getLocalBounds();
                            diffText.setOrigin(textRect.left + textRect.width / 2.0f, textRect.top + textRect.height / 2.0f);
                            diffText.setPosition(WINDOW_WIDTH / 2.0f, 200.f + i * 60.f);
                            difficultyTexts.push_back(diffText);
                        }
                    }
                    else if (event.key.code == sf::Keyboard::Escape)
                    {
                        gameState = GameState::TITLE;
                    }
                }
            }
            else if (gameState == GameState::DIFFICULTY_SELECTION)
            {
                if (event.type == sf::Event::KeyPressed)
                {
                    if (event.key.code == sf::Keyboard::Down)
                    {
                        selectedDifficultyIndex = (selectedDifficultyIndex + 1) % songs[selectedSongIndex].charts.size();
                    }
                    else if (event.key.code == sf::Keyboard::Up)
                    {
                        selectedDifficultyIndex = (selectedDifficultyIndex + songs[selectedSongIndex].charts.size() - 1) % songs[selectedSongIndex].charts.size();
                    }
                    else if (event.key.code == sf::Keyboard::Enter)
                    {
                        // --- ゲーム開始処理 ---
                        const auto& selectedSong = songs[selectedSongIndex];
                        const auto& selectedChart = selectedSong.charts[selectedDifficultyIndex];

                        if (!music.openFromFile(selectedSong.audioPath)) { return -1; }
                        chart = loadChartFromMidi(selectedChart.chartPath);
                        if (chart.empty()) { return -1; }

                        gameState = GameState::PLAYING;
                        score = 0;
                        combo = 0;
                        maxCombo = 0;
                        perfectCount = 0;
                        greatCount = 0;
                        missCount = 0;
                        nextNoteIndex = 0;
                        activeNotes.clear();
                        music.play();
                    }
                }
            }
            else if (gameState == GameState::PLAYING)
            {
                if (event.type == sf::Event::KeyPressed)
                {
                    if (event.key.code == sf::Keyboard::Escape)
                    {
                        gameState = GameState::PAUSED;
                        music.pause();
                    }
                    else
                    {
                        bool keyProcessed = false;
                        for (int i = 0; i < LANE_COUNT; ++i)
                        {
                            if (event.key.code == LANE_KEYS[i])
                            {
                                for (auto& note : activeNotes)
                                {
                                    if (!note.isProcessed && note.laneIndex == i)
                                    {
                                        float musicTime = music.getPlayingOffset().asSeconds();
                                        float diff = std::abs(musicTime - note.spawnTime);

                                        Judgment currentJudgment = Judgment::NONE;
                                        if (diff < PERFECT_WINDOW) {
                                            currentJudgment = Judgment::PERFECT;
                                            score += 100;
                                            combo++;
                                            perfectCount++;
                                        } else if (diff < GREAT_WINDOW) {
                                            currentJudgment = Judgment::GREAT;
                                            score += 50;
                                            combo++;
                                            greatCount++;
                                        }

                                        if (combo > maxCombo) {
                                            maxCombo = combo;
                                        }

                                        if (currentJudgment != Judgment::NONE) {
                                            laneFlashClocks[i].restart(); // 対応するレーンの時計をリスタート
                                            tapSound.play();
                                            note.isProcessed = true;
                                            keyProcessed = true;
                                            if(currentJudgment == Judgment::PERFECT) {
                                                judgmentText.setString("Perfect");
                                                judgmentText.setFillColor(sf::Color::Cyan);
                                            }
                                            if(currentJudgment == Judgment::GREAT) {
                                                judgmentText.setString("Great");
                                                judgmentText.setFillColor(sf::Color::Yellow);
                                            }
                                            sf::FloatRect textRect = judgmentText.getLocalBounds();
                                            judgmentText.setOrigin(textRect.left + textRect.width / 2.0f, textRect.top + textRect.height / 2.0f);
                                            judgmentText.setPosition(LANE_START_X + LANE_AREA_WIDTH / 2.f, JUDGMENT_LINE_Y - 100.f);
                                            judgmentText.setScale(1.5f, 1.5f); // アニメーションの初期スケールを設定
                                            judgmentClock.restart();
                                            break;
                                        }
                                    }
                                }
                            }
                            if(keyProcessed) break;
                        }
                    }
                }
            }
            else if (gameState == GameState::PAUSED)
            {
                if (event.type == sf::Event::KeyPressed)
                {
                    if (event.key.code == sf::Keyboard::Down)
                    {
                        selectedPauseMenuIndex = (selectedPauseMenuIndex + 1) % pauseMenuTexts.size();
                    }
                    else if (event.key.code == sf::Keyboard::Up)
                    {
                        selectedPauseMenuIndex = (selectedPauseMenuIndex + pauseMenuTexts.size() - 1) % pauseMenuTexts.size();
                    }
                    else if (event.key.code == sf::Keyboard::Enter)
                    {
                        if (selectedPauseMenuIndex == 0) // Resume
                        {
                            gameState = GameState::PLAYING;
                            music.play();
                        }
                        else if (selectedPauseMenuIndex == 1) // Retry
                        {
                            gameState = GameState::PLAYING;
                            score = 0;
                            combo = 0;
                            maxCombo = 0;
                            perfectCount = 0;
                            greatCount = 0;
                            missCount = 0;
                            nextNoteIndex = 0;
                            activeNotes.clear();
                            music.stop();
                            music.play();
                        }
                        else if (selectedPauseMenuIndex == 2) // Back to Select
                        {
                            gameState = GameState::SONG_SELECTION;
                            music.stop();
                        }
                    }
                    else if (event.key.code == sf::Keyboard::Escape)
                    {
                        gameState = GameState::PLAYING;
                        music.play();
                    }
                }
            }
            else if (gameState == GameState::RESULTS)
            {
                if (event.type == sf::Event::KeyPressed) {
                    if (event.key.code == sf::Keyboard::Right) {
                        selectedResultsMenuIndex = (selectedResultsMenuIndex + 1) % resultsMenuTexts.size();
                    } else if (event.key.code == sf::Keyboard::Left) {
                        selectedResultsMenuIndex = (selectedResultsMenuIndex + resultsMenuTexts.size() - 1) % resultsMenuTexts.size();
                    } else if (event.key.code == sf::Keyboard::Enter) {
                        if (selectedResultsMenuIndex == 0) { // Retry
                            gameState = GameState::PLAYING;
                            score = 0;
                            combo = 0;
                            maxCombo = 0;
                            perfectCount = 0;
                            greatCount = 0;
                            missCount = 0;
                            nextNoteIndex = 0;
                            activeNotes.clear();
                            music.stop();
                            music.play();
                        } else if (selectedResultsMenuIndex == 1) { // Back to Select
                            gameState = GameState::SONG_SELECTION;
                        }
                    }
                }
            }
        }

        // --- 更新処理 ---
        if (gameState == GameState::TITLE)
        {
            for(size_t i = 0; i < titleMenuTexts.size(); ++i)
            {
                if(i == selectedTitleMenuIndex)
                {
                    titleMenuTexts[i].setFillColor(sf::Color::Yellow);
                }
                else
                {
                    titleMenuTexts[i].setFillColor(sf::Color::White);
                }
            }
        }
        else if (gameState == GameState::OPTIONS)
        {
            std::stringstream ss;
            ss << std::fixed << std::setprecision(1) << config.noteSpeedMultiplier;
            speedValueText.setString(ss.str());
            textRect = speedValueText.getLocalBounds();
            speedValueText.setOrigin(textRect.left + textRect.width / 2.0f, textRect.top + textRect.height / 2.0f);
            speedValueText.setPosition(speedOptionText.getPosition().x + 300.f, speedOptionText.getPosition().y + 20.f);
        }
        else if (gameState == GameState::SONG_SELECTION)
        {
            for(size_t i = 0; i < songTitleTexts.size(); ++i)
            {
                if(i == selectedSongIndex)
                {
                    songTitleTexts[i].setFillColor(sf::Color::Yellow);
                }
                else
                {
                    songTitleTexts[i].setFillColor(sf::Color::White);
                }
            }
        }
        else if (gameState == GameState::DIFFICULTY_SELECTION)
        {
            for(size_t i = 0; i < difficultyTexts.size(); ++i)
            {
                if(i == selectedDifficultyIndex)
                {
                    difficultyTexts[i].setFillColor(sf::Color::Yellow);
                }
                else
                {
                    difficultyTexts[i].setFillColor(sf::Color::White);
                }
            }

            // ハイスコア表示
            const auto& selectedSong = songs[selectedSongIndex];
            const auto& selectedChart = selectedSong.charts[selectedDifficultyIndex];
            std::string key = generateHighScoreKey(selectedSong, selectedChart);
            int highScore = highScores.count(key) ? highScores.at(key) : 0;
            difficultyHighScoreText.setString("High Score: " + std::to_string(highScore));
            textRect = difficultyHighScoreText.getLocalBounds();
            difficultyHighScoreText.setOrigin(textRect.left + textRect.width / 2.0f, textRect.top + textRect.height / 2.0f);
            difficultyHighScoreText.setPosition(WINDOW_WIDTH / 2.0f, WINDOW_HEIGHT - 150.f);
        }
        else if (gameState == GameState::PAUSED)
        {
            for(size_t i = 0; i < pauseMenuTexts.size(); ++i)
            {
                if(i == selectedPauseMenuIndex)
                {
                    pauseMenuTexts[i].setFillColor(sf::Color::Yellow);
                }
                else
                {
                    pauseMenuTexts[i].setFillColor(sf::Color::White);
                }
            }
        }
        else if (gameState == GameState::RESULTS)
        {
            for(size_t i = 0; i < resultsMenuTexts.size(); ++i)
            {
                if(i == selectedResultsMenuIndex)
                {
                    resultsMenuTexts[i].setFillColor(sf::Color::Yellow);
                }
                else
                {
                    resultsMenuTexts[i].setFillColor(sf::Color::White);
                }
            }
        }
        else if (gameState == GameState::PLAYING)
        {
            float currentMusicTime = music.getPlayingOffset().asSeconds();

            float fallTime = JUDGMENT_LINE_Y / (NOTE_PIXELS_PER_SECOND * config.noteSpeedMultiplier);
            while (nextNoteIndex < chart.size() && chart[nextNoteIndex].spawnTime < currentMusicTime + fallTime) {
                activeNotes.push_back(chart[nextNoteIndex]);
                nextNoteIndex++;
            }

            for (auto& note : activeNotes) {
                if (!note.isProcessed) {
                    float timeUntilJudgment = note.spawnTime - currentMusicTime;
                    float newY = JUDGMENT_LINE_Y - (timeUntilJudgment * (NOTE_PIXELS_PER_SECOND * config.noteSpeedMultiplier));
                    note.shape.setPosition(note.shape.getPosition().x, newY);

                    if (timeUntilJudgment < -GREAT_WINDOW) {
                        note.isProcessed = true;
                        combo = 0;
                        missCount++;
                        judgmentText.setString("Miss");
                        judgmentText.setFillColor(sf::Color::Red);
                        sf::FloatRect textRect = judgmentText.getLocalBounds();
                        judgmentText.setOrigin(textRect.left + textRect.width / 2.0f, textRect.top + textRect.height / 2.0f);
                        judgmentText.setPosition(LANE_START_X + LANE_AREA_WIDTH / 2.f, JUDGMENT_LINE_Y - 100.f);
                        judgmentText.setScale(1.5f, 1.5f); // アニメーションの初期スケールを設定
                        judgmentClock.restart();
                    }
                }
            }
            
            activeNotes.erase(
                std::remove_if(activeNotes.begin(), activeNotes.end(), [currentMusicTime](const Note& note) {
                    return note.isProcessed && (note.spawnTime < currentMusicTime - 1.0f);
                }),
                activeNotes.end()
            );

            for (int i = 0; i < LANE_COUNT; ++i) {
                if (laneFlashClocks[i].getElapsedTime().asSeconds() < 0.1f) {
                    lanes[i].setFillColor(sf::Color::White); // ヒットした瞬間は白く光る
                } else if (sf::Keyboard::isKeyPressed(LANE_KEYS[i])) {
                    lanes[i].setFillColor(LANE_COLOR_PRESSED); // キーが押されている間は黄色
                } else {
                    lanes[i].setFillColor(LANE_COLOR_NORMAL); // 通常時は半透明の黒
                }
            }

            scoreText.setString("Score: " + std::to_string(score));
            if (combo > 2) {
                comboText.setString(std::to_string(combo));
                sf::FloatRect textRect = comboText.getLocalBounds();
                comboText.setOrigin(textRect.left + textRect.width / 2.0f, textRect.top + textRect.height / 2.0f);
                comboText.setPosition(LANE_START_X + LANE_AREA_WIDTH / 2.f, JUDGMENT_LINE_Y - 50.f);
            }

            // 判定テキストのアニメーション
            const float animationDuration = 0.2f; // アニメーションの時間（秒）
            float elapsed = judgmentClock.getElapsedTime().asSeconds();
            if (elapsed < animationDuration) {
                float scale = 1.5f - (0.5f * (elapsed / animationDuration));
                judgmentText.setScale(scale, scale);
            } else {
                judgmentText.setScale(1.0f, 1.0f);
            }

            // 曲の終了を検知
            if (music.getStatus() == sf::Music::Stopped && activeNotes.empty())
            {
                gameState = GameState::RESULTS;

                // ハイスコアのチェックと更新
                const auto& selectedSong = songs[selectedSongIndex];
                const auto& selectedChart = selectedSong.charts[selectedDifficultyIndex];
                std::string key = generateHighScoreKey(selectedSong, selectedChart);
                int oldHighScore = highScores.count(key) ? highScores.at(key) : 0;
                bool isNewRecord = score > oldHighScore;
                if (isNewRecord) {
                    highScores[key] = score;
                    saveHighScores(highScores);
                }

                // リザルトテキストの設定
                finalScoreText.setString("Score: " + std::to_string(score));
                maxComboText.setString("Max Combo: " + std::to_string(maxCombo));
                perfectCountText.setString("Perfect: " + std::to_string(perfectCount));
                greatCountText.setString("Great: " + std::to_string(greatCount));
                missCountText.setString("Miss: " + std::to_string(missCount));

                // Score and Max Combo (centered)
                textRect = finalScoreText.getLocalBounds();
                finalScoreText.setOrigin(textRect.left + textRect.width / 2.0f, textRect.top + textRect.height / 2.0f);
                finalScoreText.setPosition(WINDOW_WIDTH / 2.0f, 250.f);

                textRect = maxComboText.getLocalBounds();
                maxComboText.setOrigin(textRect.left + textRect.width / 2.0f, textRect.top + textRect.height / 2.0f);
                maxComboText.setPosition(WINDOW_WIDTH / 2.0f, 320.f);

                // Judgment counts (horizontal layout)
                const float countsY = 420.f;
                
                textRect = perfectCountText.getLocalBounds();
                perfectCountText.setOrigin(textRect.left + textRect.width / 2.0f, textRect.top + textRect.height / 2.0f);
                perfectCountText.setPosition(WINDOW_WIDTH / 4.0f, countsY);

                textRect = greatCountText.getLocalBounds();
                greatCountText.setOrigin(textRect.left + textRect.width / 2.0f, textRect.top + textRect.height / 2.0f);
                greatCountText.setPosition(WINDOW_WIDTH / 2.0f, countsY);

                textRect = missCountText.getLocalBounds();
                missCountText.setOrigin(textRect.left + textRect.width / 2.0f, textRect.top + textRect.height / 2.0f);
                missCountText.setPosition(WINDOW_WIDTH * 3.0f / 4.0f, countsY);

                if (isNewRecord) {
                    newRecordText.setString("NEW RECORD!");
                    textRect = newRecordText.getLocalBounds();
                    newRecordText.setOrigin(textRect.left + textRect.width / 2.0f, textRect.top + textRect.height / 2.0f);
                    newRecordText.setPosition(WINDOW_WIDTH / 2.0f, 180.f);
                } else {
                    newRecordText.setString(""); // 新記録でなければ何も表示しない
                }
            }
        }

        // --- 描画処理 ---
        window.clear(sf::Color::Black);

        if (gameState == GameState::TITLE)
        {
            window.draw(titleText);
            for(const auto& text : titleMenuTexts) {
                window.draw(text);
            }
        }
        else if (gameState == GameState::OPTIONS)
        {
            window.draw(optionsTitle);
            window.draw(speedOptionText);
            window.draw(speedValueText);
            window.draw(optionsHelpText);
        }
        else if (gameState == GameState::SONG_SELECTION)
        {
            window.draw(songSelectionTitle);
            for(const auto& text : songTitleTexts) {
                window.draw(text);
            }
        }
        else if (gameState == GameState::DIFFICULTY_SELECTION)
        {
            window.draw(difficultySelectionTitle);
            for(const auto& text : difficultyTexts) {
                window.draw(text);
            }
            window.draw(difficultyHighScoreText);
        }
        else if (gameState == GameState::PLAYING)
        {
            window.draw(backgroundSprite);
            for (const auto& lane : lanes) { window.draw(lane); }
            window.draw(judgmentLine);
            for (const auto& note : activeNotes) {
                if (note.shape.getPosition().y > -NOTE_HEIGHT && note.shape.getPosition().y < WINDOW_HEIGHT) {
                     if (!note.isProcessed) {
                        window.draw(note.shape);
                     }
                }
            }
            window.draw(scoreText);
            if (combo > 2) { window.draw(comboText); }
            if (judgmentClock.getElapsedTime().asSeconds() < 0.5f) {
                window.draw(judgmentText);
            }
        }
        else if (gameState == GameState::PAUSED)
        {
            // ポーズ中はプレイ画面を背景として描画
            window.draw(backgroundSprite);
            for (const auto& lane : lanes) { window.draw(lane); }
            window.draw(judgmentLine);
            for (const auto& note : activeNotes) {
                if (note.shape.getPosition().y > -NOTE_HEIGHT && note.shape.getPosition().y < WINDOW_HEIGHT) {
                    window.draw(note.shape);
                }
            }
            window.draw(scoreText);
            if (combo > 2) { window.draw(comboText); }
            if (judgmentClock.getElapsedTime().asSeconds() < 0.5f) {
                window.draw(judgmentText);
            }

            // オーバーレイとメニューを描画
            window.draw(pauseOverlay);
            window.draw(pauseTitle);
            for(const auto& text : pauseMenuTexts) {
                window.draw(text);
            }
        }
        else if (gameState == GameState::RESULTS)
        {
            window.draw(resultBackgroundSprite);
            window.draw(resultsTitle);
            window.draw(finalScoreText);
            window.draw(maxComboText);
            window.draw(perfectCountText);
            window.draw(greatCountText);
            window.draw(missCountText);
            window.draw(newRecordText);
            for(const auto& text : resultsMenuTexts) {
                window.draw(text);
            }
        }
        
        window.display();
    }

    return 0;
}