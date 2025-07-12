#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <vector>
#include <string>
#include <cmath> // for abs()
#include <algorithm> // for sort()
#include <fstream> // for file reading
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

    // --- UI要素の準備 ---
    // タイトル画面
    sf::Text titleText("Sound Game", font, 80);
    sf::FloatRect textRect = titleText.getLocalBounds();
    titleText.setOrigin(textRect.left + textRect.width / 2.0f, textRect.top + textRect.height / 2.0f);
    titleText.setPosition(WINDOW_WIDTH / 2.0f, WINDOW_HEIGHT / 2.0f - 100.f);

    sf::Text startText("Press Enter to Start", font, 30);
    textRect = startText.getLocalBounds();
    startText.setOrigin(textRect.left + textRect.width / 2.0f, textRect.top + textRect.height / 2.0f);
    startText.setPosition(WINDOW_WIDTH / 2.0f, WINDOW_HEIGHT / 2.0f + 50.f);

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

    sf::Text continueText("Press Enter to Continue", scoreFont, 30);
    continueText.setOutlineColor(sf::Color::Black);
    continueText.setOutlineThickness(2.f);
    textRect = continueText.getLocalBounds();
    continueText.setOrigin(textRect.left + textRect.width / 2.0f, textRect.top + textRect.height / 2.0f);
    continueText.setPosition(WINDOW_WIDTH / 2.0f, WINDOW_HEIGHT - 100.f);


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
                if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Enter)
                {
                    gameState = GameState::SONG_SELECTION;
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
                if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Enter)
                {
                    gameState = GameState::SONG_SELECTION;
                }
            }
        }

        // --- 更新処理 ---
        if (gameState == GameState::SONG_SELECTION)
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
        else if (gameState == GameState::PLAYING)
        {
            float currentMusicTime = music.getPlayingOffset().asSeconds();

            float fallTime = JUDGMENT_LINE_Y / NOTE_PIXELS_PER_SECOND;
            while (nextNoteIndex < chart.size() && chart[nextNoteIndex].spawnTime < currentMusicTime + fallTime) {
                activeNotes.push_back(chart[nextNoteIndex]);
                nextNoteIndex++;
            }

            for (auto& note : activeNotes) {
                if (!note.isProcessed) {
                    float timeUntilJudgment = note.spawnTime - currentMusicTime;
                    float newY = JUDGMENT_LINE_Y - (timeUntilJudgment * NOTE_PIXELS_PER_SECOND);
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
                lanes[i].setFillColor(sf::Keyboard::isKeyPressed(LANE_KEYS[i]) ? LANE_COLOR_PRESSED : LANE_COLOR_NORMAL);
            }

            scoreText.setString("Score: " + std::to_string(score));
            if (combo > 2) {
                comboText.setString(std::to_string(combo));
                sf::FloatRect textRect = comboText.getLocalBounds();
                comboText.setOrigin(textRect.left + textRect.width / 2.0f, textRect.top + textRect.height / 2.0f);
                comboText.setPosition(LANE_START_X + LANE_AREA_WIDTH / 2.f, JUDGMENT_LINE_Y - 50.f);
            }

            // 曲の終了を検知
            if (music.getStatus() == sf::Music::Stopped && activeNotes.empty())
            {
                gameState = GameState::RESULTS;
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
            }
        }

        // --- 描画処理 ---
        window.clear(sf::Color::Black);

        if (gameState == GameState::TITLE)
        {
            window.draw(titleText);
            window.draw(startText);
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
            window.draw(continueText);
        }
        
        window.display();
    }

    return 0;
}