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

#include "constants.hpp"
#include "types.hpp"
#include "file_utils.hpp"

// for convenience
using json = nlohmann::json;

void createParticleExplosion(std::vector<Particle>& particles, const sf::Vector2f& position) {
    for (int i = 0; i < 20; ++i) {
        Particle p;
        p.shape.setRadius(rand() % 3 + 1);
        p.shape.setFillColor(sf::Color(255, 255, 255, 200));
        p.shape.setPosition(position);

        float angle = (rand() % 360) * 3.14159f / 180.f;
        float speed = rand() % 100 + 50;
        p.velocity = sf::Vector2f(std::cos(angle) * speed, std::sin(angle) * speed);
        p.lifetime = sf::seconds(0.5f + (rand() % 50) / 100.f);
        particles.push_back(p);
    }
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
    sf::Font rankFont;
    if (!rankFont.loadFromFile("Evogria_Italic.otf")) { return -1; }

    sf::Texture titleBackgroundTexture;
    if (!titleBackgroundTexture.loadFromFile("img/title.png")) { return -1; }
    sf::Sprite titleBackgroundSprite;
    titleBackgroundSprite.setTexture(titleBackgroundTexture);

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

    sf::SoundBuffer menuNavigateSoundBuffer;
    if (!menuNavigateSoundBuffer.loadFromFile("audio/selection.wav")) { return -1; }

    sf::SoundBuffer missSoundBuffer;
    if (!missSoundBuffer.loadFromFile("audio/miss.wav")) { return -1; }
    sf::Sound tapSound;
    tapSound.setBuffer(tapSoundBuffer);

    sf::Sound menuNavigateSound;
    menuNavigateSound.setBuffer(menuNavigateSoundBuffer);

    sf::Sound missSound;
    missSound.setBuffer(missSoundBuffer);

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
            if (song_json.contains("background_path")) {
                song_data.backgroundPath = song_json.at("background_path").get<std::string>();
            } else {
                song_data.backgroundPath = ""; // パスがなければ空文字
            }
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

    // --- 初期音量の設定 ---
    tapSound.setVolume(config.sfxVolume);
    menuNavigateSound.setVolume(config.sfxVolume);
    missSound.setVolume(config.sfxVolume);

    // --- UI要素の準備 ---
    // タイトル画面
    sf::Text titleText("Sound Game", font, 120); // 80 -> 120
    sf::FloatRect textRect = titleText.getLocalBounds();
    titleText.setOrigin(textRect.left + textRect.width / 2.0f, textRect.top + textRect.height / 2.0f);
    titleText.setPosition(WINDOW_WIDTH / 2.0f, 350.f); // 200 -> 350

    std::vector<sf::Text> titleMenuTexts(2);
    std::vector<std::string> titleMenuStrings = {"Start Game", "Options"};
    for(size_t i = 0; i < titleMenuTexts.size(); ++i) {
        titleMenuTexts[i].setFont(font);
        titleMenuTexts[i].setCharacterSize(50); // 32 -> 50
        titleMenuTexts[i].setString(titleMenuStrings[i]);
        textRect = titleMenuTexts[i].getLocalBounds();
        titleMenuTexts[i].setOrigin(textRect.left + textRect.width / 2.0f, textRect.top + textRect.height / 2.0f);
        titleMenuTexts[i].setPosition(WINDOW_WIDTH / 2.0f, 650.f + i * 80.f); // 400, 60 -> 650, 80
    }

    // 曲選択画面
    sf::Text songSelectionTitle("Select a Song", font, 80); // 50 -> 80
    textRect = songSelectionTitle.getLocalBounds();
    songSelectionTitle.setOrigin(textRect.left + textRect.width / 2.0f, textRect.top + textRect.height / 2.0f);
    songSelectionTitle.setPosition(WINDOW_WIDTH / 2.0f, 150.f); // 80 -> 150
    std::vector<sf::Text> songTitleTexts(songs.size());
    for(size_t i = 0; i < songs.size(); ++i) {
        songTitleTexts[i].setFont(font);
        songTitleTexts[i].setCharacterSize(50); // 32 -> 50
        songTitleTexts[i].setString(songs[i].title);
        textRect = songTitleTexts[i].getLocalBounds();
        songTitleTexts[i].setOrigin(textRect.left + textRect.width / 2.0f, textRect.top + textRect.height / 2.0f);
        songTitleTexts[i].setPosition(WINDOW_WIDTH / 2.0f, 350.f + i * 80.f); // 200, 60 -> 350, 80
    }

    // 難易度選択画面
    sf::Text difficultySelectionTitle("", font, 80); // 50 -> 80
    std::vector<sf::Text> difficultyTexts;
    sf::Text difficultyHighScoreText("", scoreFont, 42); // 28 -> 42
    difficultyHighScoreText.setFillColor(sf::Color(255, 255, 100)); // Light Yellow

    // オプション画面
    sf::Text optionsTitle("Options", font, 90); // 60 -> 90
    textRect = optionsTitle.getLocalBounds();
    optionsTitle.setOrigin(textRect.left + textRect.width / 2.0f, textRect.top + textRect.height / 2.0f);
    optionsTitle.setPosition(WINDOW_WIDTH / 2.0f, 200.f); // 100 -> 200

    std::vector<sf::Text> optionMenuTexts(4);
    std::vector<std::string> optionMenuStrings = {"Note Speed", "BGM Volume", "SFX Volume", "Audio Offset"};
    for(size_t i = 0; i < optionMenuTexts.size(); ++i) {
        optionMenuTexts[i].setFont(font);
        optionMenuTexts[i].setCharacterSize(50); // 32 -> 50
        optionMenuTexts[i].setString(optionMenuStrings[i]);
        optionMenuTexts[i].setPosition(WINDOW_WIDTH / 2.0f - 400.f, 400.f + i * 100.f); // 250, 80 -> 400, 100
    }

    std::vector<sf::Text> optionValueTexts(4);
    for(size_t i = 0; i < optionValueTexts.size(); ++i) {
        optionValueTexts[i].setFont(font);
        optionValueTexts[i].setCharacterSize(50); // 32 -> 50
        optionValueTexts[i].setFillColor(sf::Color::Yellow);
    }

    sf::Text optionsHelpText("Up/Down to select, Left/Right to change, Enter to save", font, 36); // 24 -> 36
    textRect = optionsHelpText.getLocalBounds();
    optionsHelpText.setOrigin(textRect.left + textRect.width / 2.0f, textRect.top + textRect.height / 2.0f);
    optionsHelpText.setPosition(WINDOW_WIDTH / 2.0f, WINDOW_HEIGHT - 150.f); // 100 -> 150

    // ポーズ画面
    sf::RectangleShape pauseOverlay(sf::Vector2f(WINDOW_WIDTH, WINDOW_HEIGHT));
    pauseOverlay.setFillColor(sf::Color(0, 0, 0, 150)); // 半透明の黒
    sf::Text pauseTitle("PAUSED", font, 90); // 60 -> 90
    textRect = pauseTitle.getLocalBounds();
    pauseTitle.setOrigin(textRect.left + textRect.width / 2.0f, textRect.top + textRect.height / 2.0f);
    pauseTitle.setPosition(WINDOW_WIDTH / 2.0f, 300.f); // 150 -> 300

    std::vector<sf::Text> pauseMenuTexts(3);
    std::vector<std::string> pauseMenuStrings = {"Continue", "Retry", "Back to Select"};
    for(size_t i = 0; i < pauseMenuTexts.size(); ++i) {
        pauseMenuTexts[i].setFont(font);
        pauseMenuTexts[i].setCharacterSize(50); // 32 -> 50
        pauseMenuTexts[i].setString(pauseMenuStrings[i]);
        textRect = pauseMenuTexts[i].getLocalBounds();
        pauseMenuTexts[i].setOrigin(textRect.left + textRect.width / 2.0f, textRect.top + textRect.height / 2.0f);
        pauseMenuTexts[i].setPosition(WINDOW_WIDTH / 2.0f, 500.f + i * 80.f); // 280, 60 -> 500, 80
    }

    // ゲームオーバー画面
    sf::Text gameoverTitle("GAME OVER", font, 90);
    textRect = gameoverTitle.getLocalBounds();
    gameoverTitle.setOrigin(textRect.left + textRect.width / 2.0f, textRect.top + textRect.height / 2.0f);
    gameoverTitle.setPosition(WINDOW_WIDTH / 2.0f, 300.f);

    std::vector<sf::Text> gameoverMenuTexts(2);
    std::vector<std::string> gameoverMenuStrings = {"Retry", "Back to Select"};
    for(size_t i = 0; i < gameoverMenuTexts.size(); ++i) {
        gameoverMenuTexts[i].setFont(font);
        gameoverMenuTexts[i].setCharacterSize(50);
        gameoverMenuTexts[i].setString(gameoverMenuStrings[i]);
        textRect = gameoverMenuTexts[i].getLocalBounds();
        gameoverMenuTexts[i].setOrigin(textRect.left + textRect.width / 2.0f, textRect.top + textRect.height / 2.0f);
        gameoverMenuTexts[i].setPosition(WINDOW_WIDTH / 2.0f, 500.f + i * 80.f);
    }

    // ゲームプレイ画面
    sf::Text scoreText("", scoreFont, 48); // 30 -> 48
    scoreText.setPosition(20, 20); // 10, 10 -> 20, 20
    scoreText.setOutlineColor(sf::Color::Black);
    scoreText.setOutlineThickness(2.f);

    sf::Text comboText("", font, 72); // 48 -> 72
    sf::Text judgmentText("", font, 54); // 36 -> 54
    sf::Clock judgmentClock;

    // HPゲージ
    sf::RectangleShape hpGaugeBg(sf::Vector2f(300, 20));
    hpGaugeBg.setFillColor(sf::Color(50, 50, 50));
    hpGaugeBg.setOutlineColor(sf::Color::White);
    hpGaugeBg.setOutlineThickness(2.f);
    hpGaugeBg.setPosition(WINDOW_WIDTH - 320, 20);

    sf::RectangleShape hpGauge(sf::Vector2f(300, 20));
    hpGauge.setFillColor(sf::Color::Green);
    hpGauge.setPosition(WINDOW_WIDTH - 320, 20);

    // リザルト画面
    sf::Text resultsTitle("Results", scoreFont, 90); // 60 -> 90
    resultsTitle.setOutlineColor(sf::Color::Black);
    resultsTitle.setOutlineThickness(2.f);
    textRect = resultsTitle.getLocalBounds();
    resultsTitle.setOrigin(textRect.left + textRect.width / 2.0f, textRect.top + textRect.height / 2.0f);
    resultsTitle.setPosition(WINDOW_WIDTH / 2.0f, 150.f); // 100 -> 150

    sf::Text finalScoreText("", scoreFont, 60); // 40 -> 60
    finalScoreText.setOutlineColor(sf::Color::Black);
    finalScoreText.setOutlineThickness(2.f);

    sf::Text maxComboText("", scoreFont, 60); // 40 -> 60
    maxComboText.setOutlineColor(sf::Color::Black);
    maxComboText.setOutlineThickness(2.f);

    sf::Text perfectCountText("", scoreFont, 50); // 32 -> 50
    perfectCountText.setOutlineColor(sf::Color::Black);
    perfectCountText.setOutlineThickness(2.f);
    perfectCountText.setFillColor(sf::Color::Cyan);

    sf::Text greatCountText("", scoreFont, 50); // 32 -> 50
    greatCountText.setOutlineColor(sf::Color::Black);
    greatCountText.setOutlineThickness(2.f);
    greatCountText.setFillColor(sf::Color::Yellow);

    sf::Text missCountText("", scoreFont, 50); // 32 -> 50
    missCountText.setOutlineColor(sf::Color::Black);
    missCountText.setOutlineThickness(2.f);
    missCountText.setFillColor(sf::Color::Red);

    sf::Text newRecordText("", scoreFont, 60); // 40 -> 60
    newRecordText.setOutlineColor(sf::Color::Black);
    newRecordText.setOutlineThickness(2.f);
    newRecordText.setFillColor(sf::Color::Yellow);

    sf::Text rankText("", rankFont, 220); // 150 -> 220
    rankText.setOutlineColor(sf::Color::Black);
    rankText.setOutlineThickness(4.f);

    std::vector<sf::Text> resultsMenuTexts(2);
    std::vector<std::string> resultsMenuStrings = {"Retry", "Back to Select"};
    for(size_t i = 0; i < resultsMenuTexts.size(); ++i) {
        resultsMenuTexts[i].setFont(scoreFont); // フォントを変更
        resultsMenuTexts[i].setCharacterSize(50); // 32 -> 50
        resultsMenuTexts[i].setString(resultsMenuStrings[i]);
        resultsMenuTexts[i].setOutlineColor(sf::Color::Black);
        resultsMenuTexts[i].setOutlineThickness(2.f);
        textRect = resultsMenuTexts[i].getLocalBounds();
        resultsMenuTexts[i].setOrigin(textRect.left + textRect.width / 2.0f, textRect.top + textRect.height / 2.0f);
        // 横並びのレイアウトに変更
        float xPos = (WINDOW_WIDTH / (resultsMenuTexts.size() + 1.0f)) * (i + 1.0f);
        resultsMenuTexts[i].setPosition(xPos, WINDOW_HEIGHT - 150.f); // 100 -> 150
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

    sf::RectangleShape fadeOverlay(sf::Vector2f(WINDOW_WIDTH, WINDOW_HEIGHT));

    // --- ゲームの状態と変数 ---
    GameState gameState = GameState::TITLE;
    int score = 0;
    int combo = 0;
    int maxCombo = 0;
    int hp = 100;
    const int MAX_HP = 100;
    int perfectCount = 0;
    int greatCount = 0;
    int missCount = 0;
    size_t nextNoteIndex = 0;
    std::vector<Note> activeNotes;
    std::vector<Note> chart;
    sf::Music music;
    sf::Music menuMusic;
    sf::Music resultsMusic;
    sf::Music gameoverMusic;
    size_t selectedSongIndex = 0;
    size_t selectedDifficultyIndex = 0;
    size_t selectedPauseMenuIndex = 0;
    size_t selectedTitleMenuIndex = 0;
    size_t selectedResultsMenuIndex = 0;
    size_t selectedOptionsMenuIndex = 0;

    std::vector<sf::Clock> laneFlashClocks(LANE_COUNT);
    sf::Clock comboAnimationClock;
    sf::Clock fadeClock;
    std::vector<Particle> particles;

    // --- メニューBGMの再生開始 ---
    if (menuMusic.openFromFile("audio/Speder2_BellFlower.ogg")) {
        menuMusic.setLoop(true);
        menuMusic.setVolume(config.bgmVolume);
        menuMusic.play();
    }

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
                        menuNavigateSound.play();
                    } else if (event.key.code == sf::Keyboard::Up) {
                        selectedTitleMenuIndex = (selectedTitleMenuIndex + titleMenuTexts.size() - 1) % titleMenuTexts.size();
                        menuNavigateSound.play();
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
                    if (event.key.code == sf::Keyboard::Up) {
                        selectedOptionsMenuIndex = (selectedOptionsMenuIndex + optionMenuTexts.size() - 1) % optionMenuTexts.size();
                        menuNavigateSound.play();
                    } else if (event.key.code == sf::Keyboard::Down) {
                        selectedOptionsMenuIndex = (selectedOptionsMenuIndex + 1) % optionMenuTexts.size();
                        menuNavigateSound.play();
                    } else if (event.key.code == sf::Keyboard::Right) {
                        if (selectedOptionsMenuIndex == 0) { // Note Speed
                            config.noteSpeedMultiplier = std::min(5.0f, config.noteSpeedMultiplier + 0.1f);
                        } else if (selectedOptionsMenuIndex == 1) { // BGM Volume
                            config.bgmVolume = std::min(100.0f, config.bgmVolume + 5.0f);
                            menuMusic.setVolume(config.bgmVolume);
                            menuNavigateSound.play();
                        } else if (selectedOptionsMenuIndex == 2) { // SFX Volume
                            config.sfxVolume = std::min(100.0f, config.sfxVolume + 5.0f);
                            tapSound.setVolume(config.sfxVolume);
                            menuNavigateSound.setVolume(config.sfxVolume);
                            menuNavigateSound.play();
                        } else if (selectedOptionsMenuIndex == 3) { // Audio Offset
                            float increment = (sf::Keyboard::isKeyPressed(sf::Keyboard::LShift) || sf::Keyboard::isKeyPressed(sf::Keyboard::RShift)) ? 10.0f : 1.0f;
                            config.audioOffset = std::min(1000.0f, config.audioOffset + increment);
                        }
                    } else if (event.key.code == sf::Keyboard::Left) {
                        if (selectedOptionsMenuIndex == 0) { // Note Speed
                            config.noteSpeedMultiplier = std::max(0.1f, config.noteSpeedMultiplier - 0.1f);
                        } else if (selectedOptionsMenuIndex == 1) { // BGM Volume
                            config.bgmVolume = std::max(0.0f, config.bgmVolume - 5.0f);
                            menuMusic.setVolume(config.bgmVolume);
                            menuNavigateSound.play();
                        } else if (selectedOptionsMenuIndex == 2) { // SFX Volume
                            config.sfxVolume = std::max(0.0f, config.sfxVolume - 5.0f);
                            tapSound.setVolume(config.sfxVolume);
                            menuNavigateSound.setVolume(config.sfxVolume);
                            menuNavigateSound.play();
                        } else if (selectedOptionsMenuIndex == 3) { // Audio Offset
                            float decrement = (sf::Keyboard::isKeyPressed(sf::Keyboard::LShift) || sf::Keyboard::isKeyPressed(sf::Keyboard::RShift)) ? 10.0f : 1.0f;
                            config.audioOffset = std::max(-1000.0f, config.audioOffset - decrement);
                        }
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
                        menuNavigateSound.play();
                    }
                    else if (event.key.code == sf::Keyboard::Up)
                    {
                        selectedSongIndex = (selectedSongIndex + songs.size() - 1) % songs.size();
                        menuNavigateSound.play();
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
                        difficultySelectionTitle.setPosition(WINDOW_WIDTH / 2.0f, 150.f); // 80 -> 150

                        difficultyTexts.clear();
                        for(size_t i = 0; i < selectedSong.charts.size(); ++i) {
                            sf::Text diffText;
                            diffText.setFont(font);
                            diffText.setCharacterSize(50); // 32 -> 50
                            diffText.setString(selectedSong.charts[i].difficultyName);
                            textRect = diffText.getLocalBounds();
                            diffText.setOrigin(textRect.left + textRect.width / 2.0f, textRect.top + textRect.height / 2.0f);
                            diffText.setPosition(WINDOW_WIDTH / 2.0f, 350.f + i * 80.f); // 200, 60 -> 350, 80
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
                        menuNavigateSound.play();
                    }
                    else if (event.key.code == sf::Keyboard::Up)
                    {
                        selectedDifficultyIndex = (selectedDifficultyIndex + songs[selectedSongIndex].charts.size() - 1) % songs[selectedSongIndex].charts.size();
                        menuNavigateSound.play();
                    }
                    else if (event.key.code == sf::Keyboard::Enter)
                    {
                        // --- ゲーム開始処理 ---
                        const auto& selectedSong = songs[selectedSongIndex];
                        const auto& selectedChart = selectedSong.charts[selectedDifficultyIndex];

                        // 背景の更新
                        if (!selectedSong.backgroundPath.empty() && !backgroundTexture.loadFromFile(selectedSong.backgroundPath)) {
                            // 読み込み失敗時はデフォルトにフォールバック
                            backgroundTexture.loadFromFile("img/default.jpg");
                        }
                        backgroundSprite.setTexture(backgroundTexture, true);

                        if (!music.openFromFile(selectedSong.audioPath)) { return -1; }
                        music.setVolume(config.bgmVolume);
                        chart = loadChartFromMidi(selectedChart.chartPath);
                        if (chart.empty()) { return -1; }

                        menuMusic.stop(); // メニューBGMを停止
                        gameState = GameState::PLAYING;
                        score = 0;
                        combo = 0;
                        maxCombo = 0;
                        perfectCount = 0;
                        greatCount = 0;
                        missCount = 0;
                        hp = MAX_HP;
                        nextNoteIndex = 0;
                        activeNotes.clear();
                        music.play();
                    }
                    else if (event.key.code == sf::Keyboard::Escape)
                    {
                        gameState = GameState::SONG_SELECTION;
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
                                        float musicTime = music.getPlayingOffset().asSeconds() + (config.audioOffset / 1000.0f);
                                        float diff = std::abs(musicTime - note.spawnTime);

                                        Judgment currentJudgment = Judgment::NONE;
                                        if (diff < PERFECT_WINDOW) {
                                            currentJudgment = Judgment::PERFECT;
                                            score += 100;
                                            combo++;
                                            perfectCount++;
                                            hp = std::min(MAX_HP, hp + 2); // HP回復
                                        } else if (diff < GREAT_WINDOW) {
                                            currentJudgment = Judgment::GREAT;
                                            score += 50;
                                            combo++;
                                            greatCount++;
                                            hp = std::min(MAX_HP, hp + 1); // HP微回復
                                        }

                                        if (combo > maxCombo) {
                                            maxCombo = combo;
                                        }

                                        // 10コンボごとのアニメーショントリガー
                                        if (combo > 0 && combo % 10 == 0) {
                                            comboAnimationClock.restart();
                                        }

                                        if (currentJudgment != Judgment::NONE) {
                                            createParticleExplosion(particles, note.shape.getPosition()); // パーティクル生成
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
                                            judgmentText.setPosition(LANE_START_X + note.laneIndex * LANE_WIDTH + LANE_WIDTH / 2.f, JUDGMENT_LINE_Y - 100.f);
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
                        menuNavigateSound.play();
                    }
                    else if (event.key.code == sf::Keyboard::Up)
                    {
                        selectedPauseMenuIndex = (selectedPauseMenuIndex + pauseMenuTexts.size() - 1) % pauseMenuTexts.size();
                        menuNavigateSound.play();
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
                            hp = MAX_HP;
                            nextNoteIndex = 0;
                            activeNotes.clear();
                            music.stop();
                            music.setVolume(config.bgmVolume);
                            music.play();
                        }
                        else if (selectedPauseMenuIndex == 2) // Back to Select
                        {
                            gameState = GameState::SONG_SELECTION;
                            music.stop();
                            if (menuMusic.getStatus() != sf::Music::Playing) menuMusic.play();
                        }
                    }
                    else if (event.key.code == sf::Keyboard::Escape)
                    {
                        gameState = GameState::PLAYING;
                        music.play();
                    }
                }
            }
            else if (gameState == GameState::GAMEOVER)
            {
                if (event.type == sf::Event::KeyPressed)
                {
                    if (event.key.code == sf::Keyboard::Down)
                    {
                        selectedPauseMenuIndex = (selectedPauseMenuIndex + 1) % gameoverMenuTexts.size();
                        menuNavigateSound.play();
                    }
                    else if (event.key.code == sf::Keyboard::Up)
                    {
                        selectedPauseMenuIndex = (selectedPauseMenuIndex + gameoverMenuTexts.size() - 1) % gameoverMenuTexts.size();
                        menuNavigateSound.play();
                    }
                    else if (event.key.code == sf::Keyboard::Enter)
                        {
                            gameoverMusic.stop();
                            if (selectedPauseMenuIndex == 0) // Retry
                            {
                            gameState = GameState::PLAYING;
                            score = 0;
                            combo = 0;
                            maxCombo = 0;
                            perfectCount = 0;
                            greatCount = 0;
                            missCount = 0;
                            hp = MAX_HP;
                            nextNoteIndex = 0;
                            activeNotes.clear();
                            music.stop();
                            music.setVolume(config.bgmVolume);
                            music.play();
                        }
                        else if (selectedPauseMenuIndex == 1) // Back to Select
                        {
                            gameState = GameState::SONG_SELECTION;
                            music.stop();
                            if (menuMusic.getStatus() != sf::Music::Playing) menuMusic.play();
                        }
                    }
                }
            }
            else if (gameState == GameState::RESULTS)
            {
                if (event.type == sf::Event::KeyPressed) {
                    if (event.key.code == sf::Keyboard::Right) {
                        selectedResultsMenuIndex = (selectedResultsMenuIndex + 1) % resultsMenuTexts.size();
                        menuNavigateSound.play();
                    } else if (event.key.code == sf::Keyboard::Left) {
                        selectedResultsMenuIndex = (selectedResultsMenuIndex + resultsMenuTexts.size() - 1) % resultsMenuTexts.size();
                        menuNavigateSound.play();
                    } else if (event.key.code == sf::Keyboard::Enter) {
                        if (selectedResultsMenuIndex == 0) { // Retry
                            resultsMusic.stop();
                            gameState = GameState::PLAYING;
                            score = 0;
                            combo = 0;
                            maxCombo = 0;
                            perfectCount = 0;
                            greatCount = 0;
                            missCount = 0;
                            hp = MAX_HP;
                            nextNoteIndex = 0;
                            activeNotes.clear();
                            music.stop();
                            music.setVolume(config.bgmVolume);
                            music.play();
                        } else if (selectedResultsMenuIndex == 1) { // Back to Select
                            resultsMusic.stop();
                            gameState = GameState::SONG_SELECTION;
                            if (menuMusic.getStatus() != sf::Music::Playing) menuMusic.play();
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
            for(size_t i = 0; i < optionMenuTexts.size(); ++i) {
                if (i == selectedOptionsMenuIndex) {
                    optionMenuTexts[i].setFillColor(sf::Color::Yellow);
                } else {
                    optionMenuTexts[i].setFillColor(sf::Color::White);
                }
            }

            // 値のテキストを更新
            std::stringstream ss_speed, ss_bgm, ss_sfx, ss_offset;
            ss_speed << std::fixed << std::setprecision(1) << config.noteSpeedMultiplier;
            optionValueTexts[0].setString(ss_speed.str());

            ss_bgm << std::fixed << std::setprecision(0) << config.bgmVolume;
            optionValueTexts[1].setString(ss_bgm.str());

            ss_sfx << std::fixed << std::setprecision(0) << config.sfxVolume;
            optionValueTexts[2].setString(ss_sfx.str());

            ss_offset << std::fixed << std::setprecision(0) << config.audioOffset << " ms";
            optionValueTexts[3].setString(ss_offset.str());

            for(size_t i = 0; i < optionValueTexts.size(); ++i) {
                textRect = optionValueTexts[i].getLocalBounds();
                optionValueTexts[i].setOrigin(textRect.left + textRect.width, textRect.top);
                optionValueTexts[i].setPosition(optionMenuTexts[i].getPosition().x + 800.f, optionMenuTexts[i].getPosition().y); // 450 -> 800
            }
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
            difficultyHighScoreText.setPosition(WINDOW_WIDTH / 2.0f, WINDOW_HEIGHT - 200.f); // 150 -> 200
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
        else if (gameState == GameState::GAMEOVER)
        {
            for(size_t i = 0; i < gameoverMenuTexts.size(); ++i)
            {
                if(i == selectedPauseMenuIndex)
                {
                    gameoverMenuTexts[i].setFillColor(sf::Color::Yellow);
                }
                else
                {
                    gameoverMenuTexts[i].setFillColor(sf::Color::White);
                }
            }
        }
        else if (gameState == GameState::RESULTS)
        {
            const float fadeDuration = 1.0f; // 1秒でフェードイン
            float elapsed = fadeClock.getElapsedTime().asSeconds();
            if (elapsed < fadeDuration) {
                sf::Uint8 alpha = static_cast<sf::Uint8>(255 * (1 - (elapsed / fadeDuration)));
                fadeOverlay.setFillColor(sf::Color(0, 0, 0, alpha));
            } else {
                fadeOverlay.setFillColor(sf::Color(0, 0, 0, 0));
            }

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
            float adjustedMusicTime = music.getPlayingOffset().asSeconds() + (config.audioOffset / 1000.0f);

            // ノーツの出現
            float fallTime = JUDGMENT_LINE_Y / (NOTE_PIXELS_PER_SECOND * config.noteSpeedMultiplier);
            while (nextNoteIndex < chart.size() && chart[nextNoteIndex].spawnTime < adjustedMusicTime + fallTime) {
                activeNotes.push_back(chart[nextNoteIndex]);
                nextNoteIndex++;
            }

            // ノーツの更新とMiss判定
            for (auto& note : activeNotes) {
                if (!note.isProcessed) {
                    float timeUntilJudgment = note.spawnTime - adjustedMusicTime;
                    float newY = JUDGMENT_LINE_Y - (timeUntilJudgment * (NOTE_PIXELS_PER_SECOND * config.noteSpeedMultiplier));
                    note.shape.setPosition(note.shape.getPosition().x, newY);

                    if (timeUntilJudgment < -GREAT_WINDOW) {
                        note.isProcessed = true;
                        combo = 0;
                        missCount++;
                        hp -= 10; // HP減少
                        missSound.play();
                        judgmentText.setString("Miss");
                        judgmentText.setFillColor(sf::Color::Red);
                        sf::FloatRect textRect = judgmentText.getLocalBounds();
                        judgmentText.setOrigin(textRect.left + textRect.width / 2.0f, textRect.top + textRect.height / 2.0f);
                        judgmentText.setPosition(LANE_START_X + note.laneIndex * LANE_WIDTH + LANE_WIDTH / 2.f, JUDGMENT_LINE_Y - 100.f);
                        judgmentText.setScale(1.5f, 1.5f); // アニメーションの初期スケールを設定
                        judgmentClock.restart();
                    }
                }
            }
            
            // 処理済みノーツの削除
            activeNotes.erase(
                std::remove_if(activeNotes.begin(), activeNotes.end(), [adjustedMusicTime](const Note& note) {
                    return note.isProcessed && (note.spawnTime < adjustedMusicTime - 1.0f);
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
                if (combo >= 20) { // 100から変更
                    comboText.setFillColor(sf::Color::Magenta);
                    comboText.setCharacterSize(80); // 52 -> 80
                } else if (combo >= 10) { // 50から変更
                    comboText.setFillColor(sf::Color(255, 165, 0)); // Orange
                    comboText.setCharacterSize(76); // 48 -> 76
                } else {
                    comboText.setFillColor(sf::Color::White);
                    comboText.setCharacterSize(72); // 44 -> 72
                }
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

            // コンボテキストのアニメーション
            const float comboAnimationDuration = 0.2f;
            float comboElapsed = comboAnimationClock.getElapsedTime().asSeconds();
            if (comboElapsed < comboAnimationDuration) {
                float scale = 1.5f - (0.5f * (comboElapsed / comboAnimationDuration));
                comboText.setScale(scale, scale);
            } else {
                comboText.setScale(1.0f, 1.0f);
            }

            // パーティクルの更新
            for (auto it = particles.begin(); it != particles.end();) {
                it->lifetime -= sf::seconds(1.f / 120.f); // フレーム時間
                if (it->lifetime <= sf::Time::Zero) {
                    it = particles.erase(it);
                } else {
                    it->shape.move(it->velocity * (1.f / 120.f));
                    it->velocity.y += 200.f * (1.f / 120.f); // 重力
                    ++it;
                }
            }

            // HPゲージの更新
            float hpRatio = static_cast<float>(hp) / MAX_HP;
            hpGauge.setSize(sf::Vector2f(300 * hpRatio, 20));
            if (hpRatio > 0.5f) {
                hpGauge.setFillColor(sf::Color::Green);
            } else if (hpRatio > 0.2f) {
                hpGauge.setFillColor(sf::Color::Yellow);
            } else {
                hpGauge.setFillColor(sf::Color::Red);
            }

            // ゲームオーバーまたは曲の終了を検知
            if (hp <= 0) {
                music.stop();
                gameoverMusic.openFromFile("audio/failsound.ogg");
                gameoverMusic.setVolume(config.bgmVolume);
                gameoverMusic.play();
                gameState = GameState::GAMEOVER;
            } else if (music.getStatus() == sf::Music::Stopped && activeNotes.empty())
            {
                music.stop();
                resultsMusic.openFromFile("audio/result.ogg");
                resultsMusic.setVolume(config.bgmVolume);
                resultsMusic.play();
                fadeClock.restart();
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
                finalScoreText.setPosition(WINDOW_WIDTH / 2.0f, 400.f); // 250 -> 400

                textRect = maxComboText.getLocalBounds();
                maxComboText.setOrigin(textRect.left + textRect.width / 2.0f, textRect.top + textRect.height / 2.0f);
                maxComboText.setPosition(WINDOW_WIDTH / 2.0f, 500.f); // 320 -> 500

                // Judgment counts (horizontal layout)
                const float countsY = 650.f; // 420 -> 650
                
                textRect = perfectCountText.getLocalBounds();
                perfectCountText.setOrigin(textRect.left + textRect.width / 2.0f, textRect.top + textRect.height / 2.0f);
                perfectCountText.setPosition(WINDOW_WIDTH * 0.3f, countsY); // 0.25 -> 0.3

                textRect = greatCountText.getLocalBounds();
                greatCountText.setOrigin(textRect.left + textRect.width / 2.0f, textRect.top + textRect.height / 2.0f);
                greatCountText.setPosition(WINDOW_WIDTH * 0.5f, countsY); // 0.5 (center)

                textRect = missCountText.getLocalBounds();
                missCountText.setOrigin(textRect.left + textRect.width / 2.0f, textRect.top + textRect.height / 2.0f);
                missCountText.setPosition(WINDOW_WIDTH * 0.7f, countsY); // 0.75 -> 0.7

                if (isNewRecord) {
                    newRecordText.setString("NEW RECORD!");
                    textRect = newRecordText.getLocalBounds();
                    newRecordText.setOrigin(textRect.left + textRect.width / 2.0f, textRect.top + textRect.height / 2.0f);
                    newRecordText.setPosition(WINDOW_WIDTH / 2.0f, 280.f); // 180 -> 280
                } else {
                    newRecordText.setString(""); // 新記録でなければ何も表示しない
                }

                // ランク計算
                int maxScore = chart.size() * 100;
                float scoreRatio = (maxScore > 0) ? static_cast<float>(score) / maxScore : 0.0f;
                std::string rankString;
                sf::Color rankColor;
                if (scoreRatio >= 0.95f)      { rankString = "S"; rankColor = sf::Color(255, 215, 0); } // Gold
                else if (scoreRatio >= 0.90f) { rankString = "A"; rankColor = sf::Color::Yellow; }
                else if (scoreRatio >= 0.80f) { rankString = "B"; rankColor = sf::Color::Cyan; }
                else if (scoreRatio >= 0.70f) { rankString = "C"; rankColor = sf::Color::Green; }
                else                          { rankString = "D"; rankColor = sf::Color::White; }

                rankText.setString(rankString);
                rankText.setFillColor(rankColor);
                textRect = rankText.getLocalBounds();
                rankText.setOrigin(textRect.left + textRect.width / 2.0f, textRect.top + textRect.height / 2.0f);
                rankText.setPosition(WINDOW_WIDTH * 0.8f, 250.f); // 0.75, 150 -> 0.8, 250
            }
        }

        // --- 描画処理 ---
        window.clear(sf::Color::Black);

        if (gameState == GameState::TITLE)
        {
            window.draw(titleBackgroundSprite);
            window.draw(titleText);
            for(const auto& text : titleMenuTexts) {
                window.draw(text);
            }
        }
        else if (gameState == GameState::OPTIONS)
        {
            window.draw(optionsTitle);
            for(const auto& text : optionMenuTexts) {
                window.draw(text);
            }
            for(const auto& text : optionValueTexts) {
                window.draw(text);
            }
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
            for (const auto& p : particles) {
                window.draw(p.shape);
            }
            window.draw(hpGaugeBg);
            window.draw(hpGauge);
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

            window.draw(hpGaugeBg);
            window.draw(hpGauge);

            // オーバーレイとメニューを描画
            window.draw(pauseOverlay);
            window.draw(pauseTitle);
            for(const auto& text : pauseMenuTexts) {
                window.draw(text);
            }
        }
        else if (gameState == GameState::GAMEOVER)
        {
            window.draw(backgroundSprite);
            window.draw(pauseOverlay); // ポーズ画面と同じオーバーレイを使いまわす
            window.draw(gameoverTitle);
            for(const auto& text : gameoverMenuTexts) {
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
            window.draw(rankText);
            for(const auto& text : resultsMenuTexts) {
                window.draw(text);
            }
            window.draw(fadeOverlay); // 最後にフェードを描画
        }
        
        window.display();
    }

    return 0;
}