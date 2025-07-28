#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <iostream>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <string>
#include <sstream>
#include <map>
#include <algorithm>
#include <cmath>
#include <thread>
#include <iomanip>

#define MOBILE_RESOLUTION_X 720
#define MOBILE_RESOLUTION_Y 1280
#define TOUCH_THRESHOLD 10.0f

using namespace sf;
using namespace std;

// Tipos de resíduos
enum WasteType {
    PAPER,
    PLASTIC,
    METAL,
    GLASS,
    ORGANIC,
    ELECTRONIC,
    BATTERY,
    NONE
};

// Fases do jogo
enum GamePhase {
    COMMUNITY,
    INDUSTRIAL,
    MEGACENTER,
    BOSS // Nova fase
};

// --- Waste ---
class Waste {
public:
    WasteType type;
    Sprite sprite;
    Vector2f velocity;
    bool active;
    Color originalColor;

    Waste(Texture& texture, WasteType t, int phase = 0) : type(t), active(true) {
        sprite.setTexture(texture);
        sprite.setScale(0.18f, 0.18f); // Aumentado para mobile
        sprite.setPosition(rand() % (MOBILE_RESOLUTION_X - 100), -50);
        // Velocidade ajustada: base 1.5 + 0.4 * fase + aleatório
        velocity = Vector2f(0, 1.5f + phase * 0.4f + (rand() % 10) * 0.08f);
        sprite.setColor(Color::White);
        originalColor = Color::White;
    }

    // Retorna true se passou do limite
    bool update(float speedFactor = 1.0f) {
        sprite.move(velocity * speedFactor);
        if (sprite.getPosition().y > MOBILE_RESOLUTION_Y - 200) {
            active = false;
            return true;
        }
        return false;
    }
};

class PowerUp {
public:
    enum Type {
        COMBO_BOOST,
        TIME_FREEZE,
        MAGNET,
        SHIELD,
        BOSS_DAMAGE // Novo power-up para atacar o boss
    };

    Type type;
    Sprite sprite;
    CircleShape glowEffect; // Efeito de brilho ao redor
    bool active;
    float lifetime; // Tempo de vida restante

    PowerUp(Texture& texture, Type t) : type(t), active(true), lifetime(10.0f) {
        sprite.setTexture(texture);
        sprite.setScale(0.12f, 0.12f); // Aumentado para mobile
        sprite.setPosition(rand() % (MOBILE_RESOLUTION_X - 100), -50);

        // Configurar o efeito de brilho (aumentado para touch)
        glowEffect.setRadius(48.0f);
        glowEffect.setFillColor(Color(255, 255, 255, 100)); // Semi-transparente
        glowEffect.setOrigin(48.0f, 48.0f); // Centraliza o brilho
        glowEffect.setPosition(sprite.getPosition().x + 35, sprite.getPosition().y + 35);

        // Centralizar o sprite no glowEffect
        FloatRect spriteBounds = sprite.getLocalBounds();
        sprite.setOrigin(spriteBounds.width / 2, spriteBounds.height / 2);
        sprite.setPosition(glowEffect.getPosition());
    }

    // Atualiza a posição e o efeito de piscar
    bool update(float deltaTime) {
        sprite.move(0, 2.0f); // Velocidade fixa
        glowEffect.move(0, 2.0f);

        // Piscar (alternar transparência)
        int alpha = static_cast<int>(sin(lifetime * 5) * 50 + 150);
        glowEffect.setFillColor(Color(255, 255, 255, alpha));

        lifetime -= deltaTime;
        if (lifetime <= 0 || sprite.getPosition().y > MOBILE_RESOLUTION_Y - 200) {
            active = false;
            return true; // Indica que o power-up deve ser removido
        }
        return false;
    }
};

class Game {
private:
    RenderWindow window;
    Vector2f touchStartPosition;
    vector<Texture> wasteTextures;
    vector<Waste> activeWastes;
    vector<Sprite> bins;
    vector<Text> binLabels;
    map<WasteType, FloatRect> binBounds;
    map<WasteType, Texture> binTextures; 

    Font font;
    Text scoreText;
    Text phaseText;
    Text messageText;
    Text reputationText;
    Text comboText;
    Text comboMultiplierText;

    RectangleShape reputationBar;
    RectangleShape reputationBarBack;

    int score;
    int reputation;
    int phase;
    int combo;
    float spawnTimer;
    float gameTimer;
    float eventTimer;
    bool specialEvent;
    string currentEvent;

    SoundBuffer correctBuffer;
    SoundBuffer wrongBuffer;
    SoundBuffer selectBuffer; // Novo buffer para seleção
    Sound sound;

    Clock comboClock;
    Clock powerUpClock;

    // Troque Waste* selectedWaste por:
    int selectedWasteIndex = -1;

    // Novas variáveis para a tela inicial
    bool inStartScreen = true;
    RectangleShape startButton;
    Text startButtonText;
    Text gameTitle;
    Music bgMusic;

    // Variáveis para controle de som
    Texture soundOnTex, soundOffTex;
    Sprite soundIcon;
    bool soundMuted = false;
    RectangleShape volumeBar;
    RectangleShape volumeFill;
    bool volumeDragging = false;

    // --- Adicione estas variáveis na sua classe Game ---
    Texture bgCommunity, bgIndustrial, bgMegacenter, bgBoss;
    Texture playerPortraitTex, bossPortraitTex; // Texturas para retratos
    Sprite bgSprite;
    Sprite playerPortrait, bossPortrait; // Sprites para retratos
    bool inLevelTransition = false;
    bool inIntroStory = true; // Nova tela de introdução
    bool clickedToTransition = false;
    bool inBossIntro = false; // Introdução para o boss
    RectangleShape continueButton;
    Text continueButtonText;
    Text levelInfoText;
    Text storyText; // Texto para a história
    RectangleShape storyPanel; // Painel para fundo da história

    SoundBuffer victoryBuffer, defeatBuffer;
    Sound victorySound, defeatSound;
    bool inDefeatScreen = false;

    vector<PowerUp> activePowerUps; // Power-ups ativos na tela
    vector<PowerUp::Type> inventory; // Inventário do jogador (máximo de 2)
    SoundBuffer powerUpBuffer; // Som ao coletar power-up
    Sound powerUpSound;

    vector<Texture> powerUpTextures; // Texturas exclusivas para power-ups
    float powerUpSpawnTimer; // Timer para spawn de power-ups

    // Efeitos de power-up ativos
    float timeFreezeFactor;
    float timeFreezeDuration;
    int shieldCount;
    float comboBoostMultiplier;
    float comboBoostDuration;
    bool magnetActive;
    float magnetDuration;

    // Visualização de power-ups ativos
    vector<Sprite> activePowerUpIcons;
    vector<RectangleShape> activePowerUpTimers;
    vector<Text> activePowerUpCounts;

    // Novas variáveis para o texto de feedback de power-ups
    float powerUpTextTimer = 0.0f;
    float powerUpTextAlpha = 255.0f;
    float powerUpTextY = MOBILE_RESOLUTION_Y / 2;

    // --- Novas variáveis para a fase do boss ---
    bool inBossFight = false;
    int playerLife = 100;
    int bossLife = 100;
    RectangleShape playerLifeBar;
    RectangleShape bossLifeBar;
    RectangleShape playerLifeBarBack; // Fundo da barra de vida do jogador
    RectangleShape bossLifeBarBack;    // Fundo da barra de vida do boss
    int correctHitsSinceLastBossPowerUp = 0;

public:
    // --- No construtor ---
    Game() : window(VideoMode(MOBILE_RESOLUTION_X, MOBILE_RESOLUTION_Y), "Gerenciador de Reciclagem"), 
              score(0), reputation(100), phase(0), combo(0), spawnTimer(0), 
              gameTimer(0), eventTimer(0), specialEvent(false), selectedWasteIndex(-1), 
              inStartScreen(true), powerUpSpawnTimer(0), timeFreezeFactor(1.0f),
              timeFreezeDuration(0.0f), shieldCount(0), comboBoostMultiplier(1.0f),
              comboBoostDuration(0.0f), magnetActive(false), magnetDuration(0.0f),
              inBossFight(false), playerLife(100), bossLife(100), correctHitsSinceLastBossPowerUp(0) {
        window.setFramerateLimit(60);
        srand(time(0));

        // Carregar fontes
        if (!font.loadFromFile("arial.ttf")) {
            cerr << "Erro ao carregar fonte" << endl;
            if (!font.loadFromFile("C:/Windows/Fonts/arial.ttf")) {
                // Se ainda falhar, cria fonte básica
            }
        }

        // Configurar textos
        setupTexts();

        // Carregar texturas
        loadTextures();

        // Carregar texturas das lixeiras UMA VEZ
        loadBinTextures();

        // Carregar texturas dos power-ups
        loadPowerUpTextures();

        // Configurar lixeiras
        setupBins();

        // Configurar barra de reputação
        reputationBarBack.setSize(Vector2f(MOBILE_RESOLUTION_X * 0.3f, 25));
        reputationBarBack.setFillColor(Color(50, 50, 50));
        reputationBarBack.setPosition(MOBILE_RESOLUTION_X * 0.65f, 80);
        
        reputationBar.setSize(Vector2f(MOBILE_RESOLUTION_X * 0.3f, 25));
        reputationBar.setFillColor(Color(0, 200, 0));
        reputationBar.setPosition(MOBILE_RESOLUTION_X * 0.65f, 80);

        // Carregar sons
        if (!correctBuffer.loadFromFile("assets/sounds/correct.wav")) {
            cerr << "Erro ao carregar correct.wav" << endl;
        }
        if (!wrongBuffer.loadFromFile("assets/sounds/wrong.wav")) {
            cerr << "Erro ao carregar wrong.wav" << endl;
        }
        if (!selectBuffer.loadFromFile("assets/sounds/select.wav")) {
            cerr << "Erro ao carregar select.wav" << endl;
        }

        // Configurar música de fundo
        if (!bgMusic.openFromFile("assets/sounds/menu.mp3")) {
            cerr << "Erro ao carregar musica de fundo" << endl;
        } else {
            bgMusic.setLoop(true);
            bgMusic.play();
            bgMusic.setVolume(70); // Volume padrão
        }

        // Carregar texturas dos ícones de som
        if (!soundOnTex.loadFromFile("assets/textures/sound_on.png")) {
            cerr << "Erro ao carregar sound_on.png" << endl;
        }
        if (!soundOffTex.loadFromFile("assets/textures/sound_off.png")) {
            cerr << "Erro ao carregar sound_off.png" << endl;
        }
        soundIcon.setTexture(soundOnTex);
        soundIcon.setScale(0.12f, 0.12f);
        soundIcon.setPosition(MOBILE_RESOLUTION_X - 80, 30);

        // Configurar barra de volume
        volumeBar.setSize(Vector2f(150, 15));
        volumeBar.setFillColor(Color(100, 100, 100));
        volumeBar.setPosition(50, 30);
        
        volumeFill.setSize(Vector2f(105, 15)); // 70% de volume inicial
        volumeFill.setFillColor(Color(0, 200, 0));
        volumeFill.setPosition(50, 30);

        // Carregar backgrounds
        if (!bgCommunity.loadFromFile("assets/textures/bg_community.png")) {
            cerr << "Erro ao carregar bg_community.png" << endl;
        }
        if (!bgIndustrial.loadFromFile("assets/textures/bg_industrial.png")) {
            cerr << "Erro ao carregar bg_industrial.png" << endl;
        }
        if (!bgMegacenter.loadFromFile("assets/textures/bg_megacenter.png")) {
            cerr << "Erro ao carregar bg_megacenter.png" << endl;
        }
        if (!bgBoss.loadFromFile("assets/textures/bg_boss.png")) {
            cerr << "Erro ao carregar bg_boss.png" << endl;
        }
        
        // Carregar texturas para retratos
        if (!playerPortraitTex.loadFromFile("assets/textures/player_portrait.png")) {
            cerr << "Erro ao carregar player_portrait.png" << endl;
            // Fallback: criar textura verde
            playerPortraitTex.create(50, 50);
            Uint8* pixels = new Uint8[50 * 50 * 4];
            fill(pixels, pixels + 50 * 50 * 4, 0);
            for (int i = 0; i < 50 * 50 * 4; i += 4) {
                pixels[i] = 0;    // R
                pixels[i+1] = 200; // G
                pixels[i+2] = 0;   // B
                pixels[i+3] = 255; // A
            }
            playerPortraitTex.update(pixels);
            delete[] pixels;
        }
        playerPortrait.setTexture(playerPortraitTex);
        playerPortrait.setScale(0.05f, 0.05f);
        
        if (!bossPortraitTex.loadFromFile("assets/textures/boss_portrait.png")) {
            cerr << "Erro ao carregar boss_portrait.png" << endl;
            // Fallback: criar textura vermelha
            bossPortraitTex.create(50, 50);
            Uint8* pixels = new Uint8[50 * 50 * 4];
            fill(pixels, pixels + 50 * 50 * 4, 0);
            for (int i = 0; i < 50 * 50 * 4; i += 4) {
                pixels[i]   = 200;   // R
                pixels[i+1] = 0;   // G
                pixels[i+2] = 0;   // B
                pixels[i+3] = 255; // A
            }
            bossPortraitTex.update(pixels);
            delete[] pixels;
        }
        bossPortrait.setTexture(bossPortraitTex);
        if(!inBossIntro) {
            bossPortrait.setScale(0.05f, 0.05f);
        }
        
        bgSprite.setTexture(bgCommunity); // Começa na fase 1
        bgSprite.setScale(
            static_cast<float>(MOBILE_RESOLUTION_X) / bgSprite.getLocalBounds().width,
            static_cast<float>(MOBILE_RESOLUTION_Y) / bgSprite.getLocalBounds().height
        );

        // Botão de continuar (maior para touch)
        continueButton.setSize(Vector2f(MOBILE_RESOLUTION_X * 0.6f, 100));
        continueButton.setFillColor(Color(70, 130, 180));
        continueButton.setPosition(MOBILE_RESOLUTION_X * 0.2f, MOBILE_RESOLUTION_Y * 0.7f);

        continueButtonText.setFont(font);
        continueButtonText.setString("Continuar");
        continueButtonText.setCharacterSize(36);
        continueButtonText.setFillColor(Color::White);
        FloatRect btnTextBounds = continueButtonText.getLocalBounds();
        continueButtonText.setPosition(
            MOBILE_RESOLUTION_X / 2 - btnTextBounds.width / 2,
            MOBILE_RESOLUTION_Y * 0.7f + 35
        );

        levelInfoText.setFont(font);
        levelInfoText.setCharacterSize(36);
        levelInfoText.setFillColor(Color::White);
        levelInfoText.setStyle(Text::Bold);
        levelInfoText.setPosition(50, MOBILE_RESOLUTION_Y * 0.3f);

        // Configurar painel e texto para história
        storyPanel.setSize(Vector2f(MOBILE_RESOLUTION_X * 0.9f, MOBILE_RESOLUTION_Y * 0.7f));
        storyPanel.setFillColor(Color(0, 0, 0, 180));
        storyPanel.setOutlineColor(Color::White);
        storyPanel.setOutlineThickness(2);
        storyPanel.setPosition(MOBILE_RESOLUTION_X * 0.05f, MOBILE_RESOLUTION_Y * 0.15f);
        
        storyText.setFont(font);
        storyText.setCharacterSize(28);
        storyText.setFillColor(Color::White);
        storyText.setPosition(MOBILE_RESOLUTION_X * 0.1f, MOBILE_RESOLUTION_Y * 0.2f);

        // Carregar sons de vitória e derrota
        if (!victoryBuffer.loadFromFile("assets/sounds/victory.mp3")) {
            cerr << "Erro ao carregar victory.wav" << endl;
        }
        victorySound.setBuffer(victoryBuffer);

        if (!defeatBuffer.loadFromFile("assets/sounds/defeat.wav")) {
            cerr << "Erro ao carregar defeat.wav" << endl;
        }
        defeatSound.setBuffer(defeatBuffer);

        // Configurar barras de vida para o boss fight
        playerLifeBarBack.setSize(Vector2f(MOBILE_RESOLUTION_X * 0.8f, 30));
        playerLifeBarBack.setFillColor(Color(200, 200, 200)); // Fundo cinza
        playerLifeBarBack.setOutlineColor(Color::Black);
        playerLifeBarBack.setOutlineThickness(2);
        playerLifeBarBack.setPosition(MOBILE_RESOLUTION_X * 0.1f, MOBILE_RESOLUTION_Y * 0.9f);
        
        playerLifeBar.setSize(Vector2f(MOBILE_RESOLUTION_X * 0.8f, 30));
        playerLifeBar.setFillColor(Color::Green);
        playerLifeBar.setPosition(MOBILE_RESOLUTION_X * 0.1f, MOBILE_RESOLUTION_Y * 0.9f);

        bossLifeBarBack.setSize(Vector2f(MOBILE_RESOLUTION_X * 0.8f, 30));
        bossLifeBarBack.setFillColor(Color(200, 200, 200)); // Fundo cinza
        bossLifeBarBack.setOutlineColor(Color::Black);
        bossLifeBarBack.setOutlineThickness(2);
        bossLifeBarBack.setPosition(MOBILE_RESOLUTION_X * 0.1f, 50);
        
        bossLifeBar.setSize(Vector2f(MOBILE_RESOLUTION_X * 0.8f, 30));
        bossLifeBar.setFillColor(Color::Red);
        bossLifeBar.setPosition(MOBILE_RESOLUTION_X * 0.1f, 50);
    }

    void setupTexts() {
        scoreText.setFont(font);
        scoreText.setCharacterSize(36);
        scoreText.setFillColor(Color::White);
        scoreText.setPosition(20, 20);

        phaseText.setFont(font);
        phaseText.setCharacterSize(32);
        phaseText.setFillColor(Color::White);
        phaseText.setPosition(20, 70);

        messageText.setFont(font);
        messageText.setCharacterSize(42);
        messageText.setFillColor(Color::Red);
        messageText.setPosition(MOBILE_RESOLUTION_X / 2, MOBILE_RESOLUTION_Y / 2);
        messageText.setStyle(Text::Bold);
        messageText.setOrigin(messageText.getLocalBounds().width / 2, 0);

        reputationText.setFont(font);
        reputationText.setCharacterSize(32);
        reputationText.setFillColor(Color::White);
        reputationText.setPosition(MOBILE_RESOLUTION_X * 0.65f, 40);

        comboText.setFont(font);
        comboText.setCharacterSize(36);
        comboText.setFillColor(Color::White);
        comboText.setPosition(20, 120);

        comboMultiplierText.setFont(font);
        comboMultiplierText.setCharacterSize(36);
        comboMultiplierText.setFillColor(Color::Yellow);
        comboMultiplierText.setStyle(Text::Bold);
        comboMultiplierText.setPosition(150, 120);

        // Configurações para a tela inicial
        gameTitle.setFont(font);
        gameTitle.setString("Gerenciador de Reciclagem");
        gameTitle.setCharacterSize(48);
        gameTitle.setFillColor(Color::White);
        FloatRect titleBounds = gameTitle.getLocalBounds();
        gameTitle.setPosition(MOBILE_RESOLUTION_X / 2 - titleBounds.width / 2, MOBILE_RESOLUTION_Y * 0.2f);

        startButton.setSize(Vector2f(MOBILE_RESOLUTION_X * 0.6f, 120));
        startButton.setFillColor(Color(70, 130, 180));
        startButton.setPosition(MOBILE_RESOLUTION_X * 0.2f, MOBILE_RESOLUTION_Y * 0.5f);

        startButtonText.setFont(font);
        startButtonText.setString("Comecar");
        startButtonText.setCharacterSize(42);
        startButtonText.setFillColor(Color::White);
        FloatRect btnTextBounds = startButtonText.getLocalBounds();
        startButtonText.setPosition(
            MOBILE_RESOLUTION_X / 2 - btnTextBounds.width / 2,
            MOBILE_RESOLUTION_Y * 0.5f + 40
        );
    }

    void loadTextures() {
        vector<string> textureFiles = {
            "assets/textures/paper.png",
            "assets/textures/plastic.png",
            "assets/textures/metal.png",
            "assets/textures/glass.png",
            "assets/textures/organic.png",
            "assets/textures/electronic.png",
            "assets/textures/battery.png"
        };

        for (const auto& file : textureFiles) {
            Texture tex;
            if (!tex.loadFromFile(file)) {
                cerr << "Erro ao carregar textura: " << file << endl;
                // Cria textura cor de rosa para debug
                tex.create(50, 50);
                Uint8* pixels = new Uint8[50 * 50 * 4];
                for (int i = 0; i < 50*50*4; i += 4) {
                    pixels[i]   = 255; // R
                    pixels[i+1] = 0;   // G
                    pixels[i+2] = 255; // B
                    pixels[i+3] = 255; // A
                }
                tex.update(pixels);
                delete[] pixels;
            }
            wasteTextures.push_back(tex);
        }
    }

    // Carrega as texturas das lixeiras UMA ÚNICA VEZ
    void loadBinTextures() {
        vector<pair<WasteType, string>> binFiles = {
            {PAPER,      "assets/textures/paperbin.png"},
            {PLASTIC,    "assets/textures/plasticbin.png"},
            {METAL,      "assets/textures/metalbin.png"},
            {GLASS,      "assets/textures/glassbin.png"},
            {ORGANIC,    "assets/textures/organicbin.png"},
            {ELECTRONIC, "assets/textures/electronicbin.png"},
            {BATTERY,    "assets/textures/batterybin.png"}
        };

        for (const auto& [type, file] : binFiles) {
            Texture tex;
            if (!tex.loadFromFile(file)) {
                cerr << "Erro ao carregar textura da lixeira: " << file << endl;
                // Cria textura de fallback
                tex.create(70, 100);
                Uint8* pixels = new Uint8[70*100*4];
                for (int i = 0; i < 70*100*4; i += 4) {
                    pixels[i]   = 0;   // R
                    pixels[i+1] = 150; // G
                    pixels[i+2] = 0;   // B
                    pixels[i+3] = 255; // A
                }
                tex.update(pixels);
                delete[] pixels;
            }
            binTextures[type] = tex;
        }
    }

    void loadPowerUpTextures() {
        vector<string> powerUpFiles = {
            "assets/textures/combo_boost.png",
            "assets/textures/time_freeze.png",
            "assets/textures/magnet.png",
            "assets/textures/shield.png",
            "assets/textures/boss_damage.png" // Novo ícone para dano ao boss
        };

        for (const auto& file : powerUpFiles) {
            Texture tex;
            if (!tex.loadFromFile(file)) {
                cerr << "Erro ao carregar textura do power-up: " << file << endl;
                // Cria textura de fallback
                tex.create(50, 50);
                Uint8* pixels = new Uint8[50 * 50 * 4];
                for (int i = 0; i < 50 * 50 * 4; i += 4) {
                    pixels[i] = 255;     // R
                    pixels[i + 1] = 255; // G
                    pixels[i + 2] = 0;   // B
                    pixels[i + 3] = 255; // A
                }
                tex.update(pixels);
                delete[] pixels;
            }
            powerUpTextures.push_back(tex);
        }

        if (!powerUpBuffer.loadFromFile("assets/sounds/powerup.wav")) {
            cerr << "Erro ao carregar som de power-up" << endl;
        }
        powerUpSound.setBuffer(powerUpBuffer);
    }

    void setupBins() {
        bins.clear();
        binBounds.clear();
        binLabels.clear(); // Limpa os textos antigos

        // Configuração das lixeiras de acordo com a fase
        vector<WasteType> binTypes;
        if (phase == COMMUNITY) {
            binTypes = {PAPER, PLASTIC, METAL};
        } else if (phase == INDUSTRIAL) {
            binTypes = {PAPER, PLASTIC, METAL, GLASS, ORGANIC};
        } else if (phase == MEGACENTER || phase == BOSS) { // Boss usa todas as lixeiras
            binTypes = {PAPER, PLASTIC, METAL, GLASS, ORGANIC, ELECTRONIC, BATTERY};
        }

        float binWidth = 100.0f; // Aumentado para mobile
        float spacing = (MOBILE_RESOLUTION_X - (binWidth * binTypes.size())) / (binTypes.size() + 1);

        // Ajuste da posição Y: na fase do boss, subir as lixeiras
        float binY = MOBILE_RESOLUTION_Y - 250.0f; // Posicionado mais alto
        if (phase == BOSS) {
            binY = MOBILE_RESOLUTION_Y - 300.0f;
        }

        for (int i = 0; i < binTypes.size(); i++) {
            WasteType type = binTypes[i];

            if (binTextures.find(type) == binTextures.end()) {
                cerr << "Textura não encontrada para lixeira do tipo " << type << endl;
                continue;
            }

            Sprite bin;
            bin.setTexture(binTextures[type]);
            bin.setScale(0.25f, 0.25f); // Aumentado para mobile
            float x = spacing + i * (binWidth + spacing);
            bin.setPosition(x, binY); // Usando binY
            bins.push_back(bin);
            
            // Aumentar área de toque
            FloatRect bounds = bin.getGlobalBounds();
            bounds.left -= 15;
            bounds.top -= 15;
            bounds.width += 30;
            bounds.height += 30;
            binBounds[type] = bounds;

            // Nome da lixeira
            string label;
            switch (type) {
                case PAPER: label = "Papel"; break;
                case PLASTIC: label = "Plastico"; break;
                case METAL: label = "Metal"; break;
                case GLASS: label = "Vidro"; break;
                case ORGANIC: label = "Organico"; break;
                case ELECTRONIC: label = "Eletronico"; break;
                case BATTERY: label = "Bateria"; break;
                default: label = "Lixeira"; break;
            }
            Text text;
            text.setFont(font);
            text.setString(label);
            text.setCharacterSize(24); // Aumentado para mobile
            text.setFillColor(Color::White);
            text.setPosition(x + (binWidth * 0.1f), binY + 100); // Ajuste em Y também
            binLabels.push_back(text);
        }
    }

    void spawnWaste() {
        WasteType type;
        if (phase == COMMUNITY) {
            type = static_cast<WasteType>(rand() % 3);
        } else if (phase == INDUSTRIAL) {
            type = static_cast<WasteType>(rand() % 5);
        } else {
            type = static_cast<WasteType>(rand() % 7);
        }
        // Passe a fase para ajustar velocidade
        activeWastes.push_back(Waste(wasteTextures[type], type, phase));
    }

    void spawnPowerUp() {
        PowerUp::Type type;
        if (phase == BOSS && inBossFight) {
            // Só sorteia power-ups normais, exceto BOSS_DAMAGE (que é spawnado por acertos)
            type = static_cast<PowerUp::Type>(rand() % (PowerUp::BOSS_DAMAGE)); // Não inclui BOSS_DAMAGE
        } else {
            type = static_cast<PowerUp::Type>(rand() % (PowerUp::BOSS_DAMAGE)); // Não inclui BOSS_DAMAGE
        }
        activePowerUps.push_back(PowerUp(powerUpTextures[type], type));
    }

    void update() {
        float deltaTime = 1.0f / 60.0f;
        spawnTimer += deltaTime;
        gameTimer += deltaTime;
        powerUpSpawnTimer += deltaTime;

        // Atualizar efeitos de power-ups
        updatePowerUpEffects(deltaTime);

        // Atualizar resíduos e verificar se algum passou do limite
        int wastesPassed = 0;
        for (auto& waste : activeWastes) {
            if (waste.update(timeFreezeFactor)) {
                wastesPassed++;
            }
        }

        // Atualizar power-ups
        for (auto& powerUp : activePowerUps) {
            if (powerUp.update(deltaTime)) {
                powerUp.active = false;
            }
        }
        
        // Spawn de power-ups controlado por timer
        if (powerUpSpawnTimer >= 3.0f) {
            powerUpSpawnTimer = 0;
            int chance = 0;
            if (phase == COMMUNITY) {
                chance = 30; // 30% na fase 1
            } else if (phase == INDUSTRIAL) {
                chance = 50; // 50% na fase 2
            } else {
                chance = 60; // 60% na fase 3
            }
            if (rand() % 100 < chance) {
                spawnPowerUp();
            }
        }

        // Penaliza reputação por cada lixo perdido
        if (wastesPassed > 0) {
            // CORREÇÃO: shield agora funciona na fase do boss também
            if (shieldCount > 0) {
                shieldCount--;
                messageText.setString("Escudo absorveu o erro! (" + to_string(shieldCount) + " restantes)");
                messageText.setFillColor(Color::Blue);
            } 
            else if (inBossFight) {
                // Na fase do boss, lixo perdido causa dano ao jogador
                playerLife = max(0, playerLife - wastesPassed * 10);
                if (playerLife <= 0) {
                    inDefeatScreen = true;
                    inBossFight = false;
                    messageText.setString("Você perdeu para o Boss!");
                }
            } 
            else {
                reputation = max(0, reputation - wastesPassed * 7); // penalidade ajustada
                combo = 0;
                sound.setBuffer(wrongBuffer);
                sound.play();
                if (reputation <= 0) {
                    bgMusic.pause();
                    defeatSound.play();
                    inDefeatScreen = true;
                }
            }
        }

        // Remover resíduos inativos
        activeWastes.erase(remove_if(activeWastes.begin(), activeWastes.end(),
            [&](const Waste& w) {
                return !w.active;
            }), activeWastes.end());

        // Corrige o índice do lixo selecionado se necessário
        if (selectedWasteIndex >= static_cast<int>(activeWastes.size()) || selectedWasteIndex < 0) {
            selectedWasteIndex = -1;
        }

        // Remover power-ups inativos
        activePowerUps.erase(remove_if(activePowerUps.begin(), activePowerUps.end(),
            [&](const PowerUp& p) {
                return !p.active;
            }), activePowerUps.end());

        // Gerar novos resíduos
        float spawnInterval = 2.0f - phase * 0.2f;
        if (spawnTimer > spawnInterval && activeWastes.size() < 5 + phase * 2) {
            spawnWaste();
            spawnTimer = 0;
        }

        // Eventos especiais na fase 3
        if (phase == MEGACENTER && !inBossFight) {
            eventTimer += deltaTime;
            if (eventTimer > 10.0f && !specialEvent) {
                if (rand() % 100 < 30) {
                    specialEvent = true;
                    vector<string> events = {
                        "Greve dos coletores! Velocidade aumentada!",
                        "Chuva forte! Residuos perigosos aparecendo!",
                        "Falha no sistema! Combos resetados!"
                    };
                    currentEvent = events[rand() % events.size()];
                    messageText.setString(currentEvent);
                    
                    if (currentEvent.find("Velocidade") != string::npos) {
                        for (auto& waste : activeWastes) {
                            waste.velocity.y *= 1.5f;
                        }
                    }
                    else if (currentEvent.find("Combos") != string::npos) {
                        combo = 0;
                    }
                }
                eventTimer = 0;
            }
            
            if (specialEvent && eventTimer > 3.0f) {
                specialEvent = false;
            }
        }

        // Atualizar combo
        if (combo > 0 && comboClock.getElapsedTime().asSeconds() > 5.0f) {
            combo = 0;
        }

        // Atualizar textos
        ostringstream ss;
        ss << "Pontuacao: " << score;
        scoreText.setString(ss.str());
        
        ss.str("");
        ss << "Fase: ";
        if (phase == COMMUNITY) ss << "Centro Comunitario";
        else if (phase == INDUSTRIAL) ss << "Expansao Industrial";
        else if (phase == MEGACENTER) ss << "Megacentro Urbano";
        else ss << "BOSS FINAL";
        phaseText.setString(ss.str());
        
        ss.str("");
        ss << "Reputacao: " << reputation << "%";
        reputationText.setString(ss.str());
        
        reputationBar.setSize(Vector2f(MOBILE_RESOLUTION_X * 0.3f * reputation / 100.0f, 25));
        
        // Atualizar combo text
        ss.str("");
        ss << "Combo: " << combo;
        comboText.setString(ss.str());
        
        ss.str("");
        ss << "x" << fixed << setprecision(1) << comboBoostMultiplier;
        comboMultiplierText.setString(ss.str());
        comboMultiplierText.setPosition(150 + comboText.getLocalBounds().width, 120);
        
        // Atualizar mensagens temporárias e animação do texto de power-up
        if (!messageText.getString().isEmpty()) {
            powerUpTextTimer += deltaTime;
            // Fade out e movimento para cima
            if (powerUpTextTimer > 0.5f) {
                powerUpTextAlpha = max(0.0f, 255.0f - (powerUpTextTimer - 0.5f) * 255.0f);
                powerUpTextY -= deltaTime * 30.0f; // Sobe lentamente
                messageText.setFillColor(Color(
                    messageText.getFillColor().r,
                    messageText.getFillColor().g,
                    messageText.getFillColor().b,
                    static_cast<Uint8>(powerUpTextAlpha)
                ));
                messageText.setPosition(MOBILE_RESOLUTION_X / 2, powerUpTextY);
            }
            if (powerUpTextTimer > 1.5f) {
                messageText.setString(""); // Limpa após 1.5 segundos
                powerUpTextTimer = 0.0f;
                powerUpTextAlpha = 255.0f;
                powerUpTextY = MOBILE_RESOLUTION_Y / 2;
            }
        }

        // --- Verifica vitória/derrota do boss ---
        if (inBossFight) {
            if (playerLife <= 0) {
                inDefeatScreen = true;
                inBossFight = false;
                messageText.setString("Você perdeu para o Boss!");
            } else if (bossLife <= 0) {
                inBossFight = false;
                inLevelTransition = true;
                levelInfoText.setString("Parabéns! Você derrotou o Boss!");
                bgMusic.pause();
                victorySound.play();
            }
        }
    }

    void updatePowerUpEffects(float deltaTime) {
        // Atualizar efeito de congelamento
        if (timeFreezeDuration > 0) {
            timeFreezeDuration -= deltaTime;
            
            // Transição suave: 0-1 segundos: congelando, 4-5 segundos: descongelando
            if (timeFreezeDuration > 4.0f) {
                timeFreezeFactor = max(0.1f, 1.0f - (5.0f - timeFreezeDuration));
            } else if (timeFreezeDuration < 1.0f) {
                timeFreezeFactor = min(1.0f, timeFreezeDuration);
            } else {
                timeFreezeFactor = 0.1f;
            }
            
            if (timeFreezeDuration <= 0) {
                timeFreezeFactor = 1.0f;
            }
        }
        
        // Atualizar efeito de combo boost
        if (comboBoostDuration > 0) {
            comboBoostDuration -= deltaTime;
            if (comboBoostDuration <= 0) {
                comboBoostMultiplier = 1.0f;
            }
        }
        
        // Atualizar efeito de magnet
        if (magnetActive) {
                magnetDuration -= deltaTime;
                if (magnetDuration <= 0) {
                    magnetActive = false;
                } else {
                    // Atrair resíduos para as lixeiras correspondentes
                    for (auto& waste : activeWastes) {
                        if (!waste.active) continue;
                        if (binBounds.find(waste.type) != binBounds.end()) {
                            FloatRect binRect = binBounds[waste.type];
                            Vector2f target(binRect.left + binRect.width/2, binRect.top + binRect.height/2);

                            // Move diretamente para o centro da lixeira se estiver próximo ou se magnet estiver ativo
                            Vector2f direction = target - waste.sprite.getPosition();
                            float distance = sqrt(direction.x*direction.x + direction.y*direction.y);

                            // Se estiver longe, move gradualmente
                            if (distance > 10.0f) {
                                direction = direction / distance; // Normaliza
                                waste.velocity = direction * 8.0f; // Move rápido para garantir acerto
                            } else {
                                // Coleta automaticamente
                                waste.sprite.setPosition(target);
                                int points = static_cast<int>(5 * comboBoostMultiplier);
                                score += points;
                                combo++;
                                comboClock.restart();
                                
                                // Na fase do boss, acertos recuperam vida
                                if (inBossFight) {
                                    playerLife = min(100, playerLife + 5);
                                } else {
                                    reputation = min(100, reputation + 2);
                                }
                                
                                waste.active = false;
                                waste.sprite.setColor(Color(100, 250, 100)); // Verde claro
                                sound.setBuffer(correctBuffer);
                                sound.play();
                            }
                        }
                    }
                }
        }
    }

    void handleClick(Vector2f touchPos) {
        // Se nenhum lixo está selecionado, procura por um para selecionar
        if (selectedWasteIndex == -1) {
            for (size_t i = 0; i < activeWastes.size(); ++i) {
                // Aumentar área de toque para resíduos
                FloatRect bounds = activeWastes[i].sprite.getGlobalBounds();
                bounds.left -= 20;
                bounds.top -= 20;
                bounds.width += 40;
                bounds.height += 40;
                
                if (bounds.contains(touchPos)) {
                    selectedWasteIndex = static_cast<int>(i);
                    activeWastes[i].sprite.setColor(Color(255, 255, 0));
                    sound.setBuffer(selectBuffer);
                    sound.play();
                    break;
                }
            }
        }
        // Se já existe um lixo selecionado
        else {
            bool clickedAnotherWaste = false;
            // Verifica se clicou em outro lixo
            for (size_t i = 0; i < activeWastes.size(); ++i) {
                // Aumentar área de toque para resíduos
                FloatRect bounds = activeWastes[i].sprite.getGlobalBounds();
                bounds.left -= 20;
                bounds.top -= 20;
                bounds.width += 40;
                bounds.height += 40;
                
                if (bounds.contains(touchPos)) {
                    // Remove destaque do anterior
                    if (selectedWasteIndex >= 0 && selectedWasteIndex < static_cast<int>(activeWastes.size())) {
                        activeWastes[selectedWasteIndex].sprite.setColor(Color::White);
                    }
                    // Seleciona o novo
                    selectedWasteIndex = static_cast<int>(i);
                    activeWastes[i].sprite.setColor(Color(255, 255, 0));
                    sound.setBuffer(selectBuffer);
                    sound.play();
                    clickedAnotherWaste = true;
                    break;
                }
            }
            // Se não clicou em outro lixo, tenta jogar na lixeira
            if (!clickedAnotherWaste) {
                bool clickedBin = false;
                WasteType selectedType = activeWastes[selectedWasteIndex].type;

                for (const auto& bin : binBounds) {
                    if (bin.second.contains(touchPos)) {
                        clickedBin = true;
                        bool correct = (bin.first == selectedType);

                        if (correct) {
                            sound.setBuffer(correctBuffer);
                            sound.play();

                            int points = 5 + combo;
                            points = static_cast<int>(points * comboBoostMultiplier);
                            score += points;

                            combo++;
                            comboClock.restart();
                            
                            // Na fase do boss, acertos recuperam vida
                            if (inBossFight) {
                                playerLife = min(100, playerLife + 5);
                            } else {
                                reputation = min(100, reputation + 2);
                            }

                            if (inBossFight) {
                                correctHitsSinceLastBossPowerUp++;
                                if (correctHitsSinceLastBossPowerUp >= 5) {
                                    spawnBossPowerUp();
                                    correctHitsSinceLastBossPowerUp = 0;
                                }
                            }
                        } else {
                            if (inBossFight) {
                                playerLife = max(0, playerLife - 10);
                            } else {
                                reputation = max(0, reputation - 10);
                            }
                            sound.setBuffer(wrongBuffer);
                            sound.play();
                            combo = 0;
                            if (!inBossFight && reputation <= 0) {
                                bgMusic.pause();
                                defeatSound.play();
                                inDefeatScreen = true;
                            }
                        }

                        // Remover apenas o item selecionado
                        if (selectedWasteIndex >= 0 && selectedWasteIndex < static_cast<int>(activeWastes.size())) {
                            activeWastes.erase(activeWastes.begin() + selectedWasteIndex);
                        }
                        selectedWasteIndex = -1;
                        break; // Sai do loop das lixeiras
                    }
                }
                // Se não clicou em lixeira, apenas desmarca o lixo selecionado e não faz nada
                if (!clickedBin) {
                    if (selectedWasteIndex >= 0 && selectedWasteIndex < static_cast<int>(activeWastes.size())) {
                        activeWastes[selectedWasteIndex].sprite.setColor(Color::White);
                    }
                    selectedWasteIndex = -1;
                }
            }
        }
    }

    void spawnBossPowerUp() {
        if (phase == BOSS && inBossFight) {
            activePowerUps.push_back(PowerUp(powerUpTextures[PowerUp::BOSS_DAMAGE], PowerUp::BOSS_DAMAGE));
        }
    }

    void handlePowerUpClick(Vector2f touchPos) {
        for (size_t i = 0; i < activePowerUps.size(); ++i) {
            // Verifica se o clique foi no efeito de brilho (maior e mais fácil de clicar)
            if (activePowerUps[i].glowEffect.getGlobalBounds().contains(touchPos)) {
                if (activePowerUps[i].type == PowerUp::BOSS_DAMAGE && inBossFight) {
                    bossLife = max(0, bossLife - 25); // Aumenta o dano ao boss
                    messageText.setString("Ataque no Boss! -25 HP");
                    messageText.setFillColor(Color::Red);
                } else {
                    usePowerUp(activePowerUps[i].type);
                }
                powerUpSound.play();
                activePowerUps[i].active = false;
                break;
            }
        }
    }

    // --- Adicione uma função para atualizar o background conforme a fase ---
    void updateBackground() {
        if (phase == BOSS) {
            bgSprite.setTexture(bgBoss, true);
        } else if (phase == COMMUNITY) {
            bgSprite.setTexture(bgCommunity, true);
        } else if (phase == INDUSTRIAL) {
            bgSprite.setTexture(bgIndustrial, true);
        } else {
            bgSprite.setTexture(bgMegacenter, true);
        }

        // Ajusta o tamanho do background para preencher a janela
        bgSprite.setScale(
            static_cast<float>(MOBILE_RESOLUTION_X) / bgSprite.getLocalBounds().width,
            static_cast<float>(MOBILE_RESOLUTION_Y) / bgSprite.getLocalBounds().height
        );
    }

    // --- Modifique checkPhaseTransition para mostrar a tela de transição ---
    void checkPhaseTransition() {
        if (phase == COMMUNITY && score >= 60 && !inLevelTransition) {
            inLevelTransition = true;
            levelInfoText.setString("Fase 1 completa!\nPontuacao: " + to_string(score) +
                               "\nReputacao: " + to_string(reputation) + "%");
            bgMusic.pause();
            victorySound.play();
        } else if (phase == INDUSTRIAL && score >= 120 && !inLevelTransition) {
            inLevelTransition = true;
            levelInfoText.setString("Fase 2 completa!\nPontuacao: " + to_string(score) +
                               "\nReputacao: " + to_string(reputation) + "%");
            bgMusic.pause();
            victorySound.play();
        } else if (phase == MEGACENTER && score >= 240 && !inLevelTransition) {
            inLevelTransition = true;
            levelInfoText.setString("Boss Fight!\nPrepare-se para o desafio final!");
            bgMusic.pause();
            victorySound.play();
        }
    }

    void renderDefeatScreen() {
        window.clear(Color(30, 0, 0));
        // Fundo escuro
        RectangleShape overlay(Vector2f(MOBILE_RESOLUTION_X, MOBILE_RESOLUTION_Y));
        overlay.setFillColor(Color(0, 0, 0, 200));
        window.draw(bgSprite);
        window.draw(overlay);

        Text defeatText;
        defeatText.setFont(font);
        defeatText.setString("Derrota!\nSua reputacao chegou a zero.\nToque para voltar ao menu.");
        defeatText.setCharacterSize(42);
        defeatText.setFillColor(Color::Red);
        defeatText.setStyle(Text::Bold);
        defeatText.setOrigin(defeatText.getLocalBounds().width / 2, 0);
        defeatText.setPosition(MOBILE_RESOLUTION_X / 2, MOBILE_RESOLUTION_Y * 0.3f);
        window.draw(defeatText);

        window.display();
    }

    // --- Adicione uma função para resetar o jogo ---
    void resetGame() {
        score = 0;
        reputation = 100;
        phase = 0;
        combo = 0;
        spawnTimer = 0;
        gameTimer = 0;
        eventTimer = 0;
        powerUpSpawnTimer = 0;
        specialEvent = false;
        selectedWasteIndex = -1;
        activeWastes.clear();
        activePowerUps.clear();
        inventory.clear();
        inLevelTransition = false;
        inDefeatScreen = false;
        inBossIntro = false;
        timeFreezeFactor = 1.0f;
        timeFreezeDuration = 0.0f;
        shieldCount = 0;
        comboBoostMultiplier = 1.0f;
        comboBoostDuration = 0.0f;
        magnetActive = false;
        magnetDuration = 0.0f;
        inBossFight = false;
        playerLife = 100;
        bossLife = 100;
        correctHitsSinceLastBossPowerUp = 0;
        setupBins();
        updateBackground();
    }

    void renderPowerUps() {
        for (const auto& powerUp : activePowerUps) {
            window.draw(powerUp.glowEffect);
            window.draw(powerUp.sprite);
        }
    }

    void usePowerUp(PowerUp::Type type) {
        switch (type) {
            case PowerUp::COMBO_BOOST:
                comboBoostMultiplier = 3.0f;
                comboBoostDuration = 10.0f;
                messageText.setString("Combo Boost Ativado! Pontos triplicados!");
                messageText.setFillColor(Color::Yellow);
                break;

            case PowerUp::TIME_FREEZE:
                timeFreezeDuration = 5.0f;
                messageText.setString("Time Freeze Ativado! Velocidade reduzida!");
                messageText.setFillColor(Color::Cyan);
                break;

            case PowerUp::MAGNET:
                magnetActive = true;
                magnetDuration = 5.0f;
                messageText.setString("Magnet Ativado! Residuos sendo atraidos!");
                messageText.setFillColor(Color::Green);
                break;

            case PowerUp::SHIELD:
                shieldCount += 3;
                messageText.setString("Shield Ativado! +3 escudos de protecao!");
                messageText.setFillColor(Color::Blue);
                break;
        }
        messageText.setStyle(Text::Bold);
        messageText.setOrigin(messageText.getLocalBounds().width / 2, 0);
        messageText.setPosition(MOBILE_RESOLUTION_X / 2, powerUpTextY);
        powerUpTextTimer = 0.0f;
        powerUpTextAlpha = 255.0f;
        powerUpTextY = MOBILE_RESOLUTION_Y / 2;
        comboClock.restart();
    }

    void renderActivePowerUpEffects() {
        float x = MOBILE_RESOLUTION_X * 0.8f;
        float y = 150;
        float spacing = 60;

        // Time Freeze
        if (timeFreezeDuration > 0) {
            Sprite icon;
            icon.setTexture(powerUpTextures[PowerUp::TIME_FREEZE]);
            icon.setScale(0.08f, 0.08f);
            icon.setPosition(x, y);
            window.draw(icon);

            // Barra de tempo
            RectangleShape barBack(Vector2f(60, 8));
            barBack.setFillColor(Color(50,50,50));
            barBack.setPosition(x + 70, y + 30);
            window.draw(barBack);

            float ratio = timeFreezeDuration / 5.0f;
            RectangleShape bar(Vector2f(60 * ratio, 8));
            bar.setFillColor(Color::Cyan);
            bar.setPosition(x + 70, y + 30);
            window.draw(bar);
            
            y += spacing;
        }

        // Combo Boost
        if (comboBoostDuration > 0) {
            Sprite icon;
            icon.setTexture(powerUpTextures[PowerUp::COMBO_BOOST]);
            icon.setScale(0.08f, 0.08f);
            icon.setPosition(x, y);
            window.draw(icon);

            // Barra de tempo
            RectangleShape barBack(Vector2f(60, 8));
            barBack.setFillColor(Color(50,50,50));
            barBack.setPosition(x + 70, y + 30);
            window.draw(barBack);

            float ratio = comboBoostDuration / 10.0f;
            RectangleShape bar(Vector2f(60 * ratio, 8));
            bar.setFillColor(Color::Yellow);
            bar.setPosition(x + 70, y + 30);
            window.draw(bar);
            
            // Multiplicador
            Text multText;
            multText.setFont(font);
            multText.setString("x" + to_string(static_cast<int>(comboBoostMultiplier)));
            multText.setCharacterSize(30);
            multText.setFillColor(Color::Yellow);
            multText.setPosition(x + 140, y);
            window.draw(multText);
            
            y += spacing;
        }

        // Magnet
        if (magnetActive) {
            Sprite icon;
            icon.setTexture(powerUpTextures[PowerUp::MAGNET]);
            icon.setScale(0.08f, 0.08f);
            icon.setPosition(x, y);
            window.draw(icon);

            // Barra de tempo
            RectangleShape barBack(Vector2f(60, 8));
            barBack.setFillColor(Color(50,50,50));
            barBack.setPosition(x + 70, y + 30);
            window.draw(barBack);

            float ratio = magnetDuration / 5.0f;
            RectangleShape bar(Vector2f(60 * ratio, 8));
            bar.setFillColor(Color::Green);
            bar.setPosition(x + 70, y + 30);
            window.draw(bar);
            
            y += spacing;
        }

        // Shield
        if (shieldCount > 0) {
            Sprite icon;
            icon.setTexture(powerUpTextures[PowerUp::SHIELD]);
            icon.setScale(0.08f, 0.08f);
            icon.setPosition(x, y);
            window.draw(icon);

            // Contador
            Text countText;
            countText.setFont(font);
            countText.setString(to_string(shieldCount));
            countText.setCharacterSize(36);
            countText.setFillColor(Color::Blue);
            countText.setStyle(Text::Bold);
            countText.setPosition(x + 60, y - 10);
            window.draw(countText);
        }
    }

    void renderLifeBars() {
        if (inBossFight) {
            // Atualiza tamanho das barras
            playerLifeBar.setSize(Vector2f(MOBILE_RESOLUTION_X * 0.8f * playerLife / 100.0f, 30));
            bossLifeBar.setSize(Vector2f(MOBILE_RESOLUTION_X * 0.8f * bossLife / 100.0f, 30));
            
            // Desenha fundos
            window.draw(playerLifeBarBack);
            window.draw(bossLifeBarBack);
            
            // Desenha barras de vida
            window.draw(bossLifeBar);
            window.draw(playerLifeBar);
            
            // Desenha retratos ao lado das barras
            bossPortrait.setPosition(30, 40);
            window.draw(bossPortrait);
            
            playerPortrait.setPosition(30, MOBILE_RESOLUTION_Y * 0.9f - 15);
            window.draw(playerPortrait);
            
            // Desenha textos de vida
            Text bossLifeText;
            bossLifeText.setFont(font);
            bossLifeText.setString(to_string(bossLife) + "%");
            bossLifeText.setCharacterSize(30);
            bossLifeText.setFillColor(Color::White);
            bossLifeText.setStyle(Text::Bold);
            bossLifeText.setPosition(MOBILE_RESOLUTION_X - 100, 55);
            window.draw(bossLifeText);
            
            Text playerLifeText;
            playerLifeText.setFont(font);
            playerLifeText.setString(to_string(playerLife) + "%");
            playerLifeText.setCharacterSize(30);
            playerLifeText.setFillColor(Color::White);
            playerLifeText.setStyle(Text::Bold);
            playerLifeText.setPosition(MOBILE_RESOLUTION_X - 100, MOBILE_RESOLUTION_Y * 0.9f);
            window.draw(playerLifeText);
        }
    }

    void run() {
        while (window.isOpen()) {
            Event event;
            while (window.pollEvent(event)) {
                if (event.type == Event::Closed) {
                    window.close();
                }
                
                // Controle de som na tela inicial
                if (inStartScreen) {
                    if (event.type == Event::TouchBegan) {
                        Vector2f touchPos(event.touch.x, event.touch.y);
                        if (soundIcon.getGlobalBounds().contains(touchPos)) {
                            soundMuted = !soundMuted;
                            if (soundMuted) {
                                bgMusic.setVolume(0);
                                sound.setVolume(0);
                                victorySound.setVolume(0);
                                defeatSound.setVolume(0);
                                powerUpSound.setVolume(0);
                                soundIcon.setTexture(soundOffTex);
                            } else {
                                bgMusic.setVolume(70);
                                sound.setVolume(100);
                                victorySound.setVolume(100);
                                defeatSound.setVolume(100);
                                powerUpSound.setVolume(100);
                                soundIcon.setTexture(soundOnTex);
                            }
                        }
                        // Controle de volume
                        else if (volumeBar.getGlobalBounds().contains(touchPos)) {
                            float volumePercent = (touchPos.x - volumeBar.getPosition().x) / volumeBar.getSize().x * 100.0f;
                            volumePercent = max(0.0f, min(100.0f, volumePercent));
                            volumeFill.setSize(Vector2f(volumePercent * volumeBar.getSize().x / 100.0f, 15));
                            bgMusic.setVolume(volumePercent);
                            volumeDragging = true;
                        }
                        else if (startButton.getGlobalBounds().contains(touchPos)) {
                            inStartScreen = false;
                            resetGame();
                            bgMusic.play();
                        }
                    }
                    else if (event.type == Event::TouchMoved && volumeDragging) {
                        Vector2f touchPos(event.touch.x, event.touch.y);
                        float volumePercent = (touchPos.x - volumeBar.getPosition().x) / volumeBar.getSize().x * 100.0f;
                        volumePercent = max(0.0f, min(100.0f, volumePercent));
                        volumeFill.setSize(Vector2f(volumePercent * volumeBar.getSize().x / 100.0f, 15));
                        bgMusic.setVolume(volumePercent);
                    }
                    else if (event.type == Event::TouchEnded) {
                        volumeDragging = false;
                    }
                }

                
                if (inIntroStory && event.type == Event::TouchBegan) {
                    inIntroStory = false;
                }
                
                // Tela de introdução do boss
                if (inBossIntro && event.type == Event::TouchBegan) {
                    inBossIntro = false;
                    inBossFight = true;
                }
                
                // --- Evento para continuar na tela de transição ---
                if (inLevelTransition && event.type == Event::TouchBegan) {
                    Vector2f touchPos(event.touch.x, event.touch.y);
                    if (continueButton.getGlobalBounds().contains(touchPos)) {
                        inLevelTransition = false;
                        activeWastes.clear();
                        combo = 0;
                        selectedWasteIndex = -1;
                        if (phase == COMMUNITY) {
                            phase = INDUSTRIAL;
                            setupBins();
                            updateBackground();
                            bgMusic.play();
                        } else if (phase == INDUSTRIAL) {
                            phase = MEGACENTER;
                            setupBins();
                            updateBackground();
                            bgMusic.play();
                        } else if (phase == MEGACENTER) {
                            phase = BOSS;
                            setupBins();
                            updateBackground();
                            inBossIntro = true; // Mostra introdução antes do boss
                            bgMusic.play();
                        } else if (phase == BOSS) {
                            inStartScreen = true;
                            resetGame();
                            bgMusic.play();
                        }
                    }
                }
                // --- Evento para derrota: toque para voltar ao menu ---
                if (inDefeatScreen && event.type == Event::TouchBegan) {
                    inDefeatScreen = false;
                    inStartScreen = true;
                    resetGame();
                    bgMusic.play();
                }
                if (!inStartScreen && !inLevelTransition && !inDefeatScreen && 
                    !inIntroStory && !inBossIntro && event.type == Event::TouchBegan) {
                    Vector2f touchPos(event.touch.x, event.touch.y);

                    // Primeiro verifica power-ups
                    handlePowerUpClick(touchPos);

                    // Depois verifica lixos e lixeiras
                    handleClick(touchPos);
                }
            }

            window.clear(Color(30, 70, 40));

            if (inStartScreen) {
                bgSprite.setTexture(bgCommunity, true);
                bgSprite.setScale(
                    static_cast<float>(MOBILE_RESOLUTION_X) / bgSprite.getLocalBounds().width,
                    static_cast<float>(MOBILE_RESOLUTION_Y) / bgSprite.getLocalBounds().height
                );
                window.draw(bgSprite);

                RectangleShape overlay(Vector2f(MOBILE_RESOLUTION_X, MOBILE_RESOLUTION_Y));
                overlay.setFillColor(Color(0, 0, 0, 100));
                window.draw(overlay);

                window.draw(gameTitle);
                window.draw(startButton);
                window.draw(startButtonText);
                window.draw(soundIcon);
                
                // Desenhar barra de volume
                window.draw(volumeBar);
                window.draw(volumeFill);
                
                // Texto de volume
                Text volumeText;
                volumeText.setFont(font);
                volumeText.setString("Volume");
                volumeText.setCharacterSize(24);
                volumeText.setFillColor(Color::White);
                volumeText.setPosition(50, 10);
                window.draw(volumeText);
                
                window.display();
                continue;
            }

            // --- Tela de introdução da história ---
            if (inIntroStory) {
                window.clear(Color(40, 40, 60));
                
                // Desenhar fundo temático
                RectangleShape storyBg(Vector2f(MOBILE_RESOLUTION_X, MOBILE_RESOLUTION_Y));
                storyBg.setFillColor(Color(30, 50, 70));
                window.draw(storyBg);
                
                // Painel para texto
                storyPanel.setFillColor(Color(0, 0, 0, 180));
                window.draw(storyPanel);
                
                // Texto da história
                storyText.setString(
                    "Bem-vindo ao Gerenciador de Reciclagem!\n\n"
                    "Voce e o novo coletor de lixo da cidade.\n"
                    "Nosso planeta esta sendo sufocado por residuos,\n"
                    "e cabe a voce organizar a coleta seletiva.\n\n"
                    "Sua missao:\n"
                    "Classificar corretamente os residuos que estao caindo\n"
                    "dos caminhoes de coleta antes que poluam a cidade.\n\n"
                    "Cada acerto aumenta sua reputacao e pontos.\n"
                    "Erros ou residuos perdidos diminuem sua reputacao.\n\n"
                    "Toque para comecar sua jornada!"
                );
                window.draw(storyText);
                
                window.display();
                continue;
            }
            
            // --- Tela de introdução do boss ---
            if (inBossIntro) {
                window.clear(Color(70, 30, 30));
                
                // Desenhar fundo temático
                RectangleShape storyBg(Vector2f(MOBILE_RESOLUTION_X, MOBILE_RESOLUTION_Y));
                storyBg.setFillColor(Color(60, 30, 40));
                window.draw(storyBg);
                
                // Painel para texto
                storyPanel.setFillColor(Color(0, 0, 0, 180));
                window.draw(storyPanel);
                
                // Texto da história do boss
                storyText.setString(
                    "BOSS FINAL: A INDUSTRIA POLUIDORA\n\n"
                    "Voce chegou ao desafio final!\n"
                    "A Industria Poluidora, liderada pelo CEO Ganancioso,\n"
                    "esta despejando residuos toxicos em massa!\n\n"
                    "Sua missao agora e pessoal:\n"
                    "Derrote a Industria Poluidora antes que ela destrua\n"
                    "todos os seus esforcos de reciclagem!\n\n"
                    "Diferente das fases anteriores:\n"
                    "- Cada erro reduz sua barra de vida\n"
                    "- Acertos recuperam um pouco de vida\n"
                    "- A cada 5 acertos, aparece um power-up especial\n"
                    "  que pode causar dano direto ao boss\n\n\n"
                    "Toque para comecar o combate final!"
                );
                window.draw(storyText);
                
                // Desenhar retrato do boss
                bossPortrait.setPosition(MOBILE_RESOLUTION_X - 100, MOBILE_RESOLUTION_Y * 0.6f);
                bossPortrait.setScale(0.05f, 0.05f);
                window.draw(bossPortrait);
                
                window.display();
                continue;
            }

            // --- Tela de derrota ---
            if (inDefeatScreen) {
                renderDefeatScreen();
                continue;
            }

            // --- Tela de transição de fase ---
            if (inLevelTransition) {
                window.draw(bgSprite);
                window.draw(levelInfoText);
                window.draw(continueButton);
                window.draw(continueButtonText);
                window.display();
                continue;
            }

            update();
            checkPhaseTransition();

            updateBackground();
            window.draw(bgSprite);

            for (const auto& bin : bins) {
                window.draw(bin);
            }
            for (const auto& label : binLabels) {
                window.draw(label);
            }
            for (const auto& waste : activeWastes) {
                window.draw(waste.sprite);
            }
            renderPowerUps();
            
            // Reposicionar textos na fase do boss
            if (inBossFight) {
                // Salvar posições originais
                Vector2f originalScorePos = scoreText.getPosition();
                Vector2f originalComboPos = comboText.getPosition();
                Vector2f originalPhasePos = phaseText.getPosition();

                // Calcular novas posições no canto superior direito
                FloatRect scoreBounds = scoreText.getLocalBounds();
                FloatRect comboBounds = comboText.getLocalBounds();
                FloatRect phaseBounds = phaseText.getLocalBounds();

                // Ajustar as posições para o canto direito
                float rightMargin = 20.0f; // 20 pixels da borda direita
                scoreText.setPosition(MOBILE_RESOLUTION_X - scoreBounds.width - rightMargin, 40);
                comboText.setPosition(MOBILE_RESOLUTION_X - comboBounds.width - rightMargin, 90);
                phaseText.setPosition(MOBILE_RESOLUTION_X - phaseBounds.width - rightMargin, 140);

                window.draw(scoreText);
                window.draw(comboText);
                window.draw(phaseText); // Desenhar a fase também

                // Restaurar posições originais
                scoreText.setPosition(originalScorePos);
                comboText.setPosition(originalComboPos);
                phaseText.setPosition(originalPhasePos);
            } else {
                window.draw(scoreText);
                window.draw(phaseText);
                window.draw(comboText);
            }
            
            // Não mostrar reputação na fase do boss
            if (!inBossFight) {
                window.draw(reputationText);
                window.draw(reputationBarBack);
                window.draw(reputationBar);
            }
            
            if (comboBoostMultiplier > 1.0f) {
                window.draw(comboMultiplierText);
            }

            // Desenhar efeitos visuais para power-ups ativos
            renderActivePowerUpEffects();

            if (specialEvent || !messageText.getString().isEmpty()) {
                window.draw(messageText);
            }
            
            if (inBossFight) {
                renderLifeBars();
            }

            window.display();
        }
    }
};

int main() {
    Game game;
    game.run();
    return 0;
}