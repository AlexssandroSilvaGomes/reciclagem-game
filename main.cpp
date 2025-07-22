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
        sprite.setScale(0.12f, 0.12f);
        sprite.setPosition(rand() % 600, -50);
        // Velocidade ajustada: base 1.5 + 0.4 * fase + aleatório
        velocity = Vector2f(0, 1.5f + phase * 0.4f + (rand() % 10) * 0.08f);
        sprite.setColor(Color::White);
        originalColor = Color::White;
    }

    // Retorna true se passou do limite
    bool update(float speedFactor = 1.0f) {
        sprite.move(velocity * speedFactor);
        if (sprite.getPosition().y > 600) {
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
        sprite.setScale(0.08f, 0.08f); // Reduz o tamanho do power-up
        sprite.setPosition(rand() % 600, -50);

        // Configurar o efeito de brilho
        glowEffect.setRadius(32.0f);
        glowEffect.setFillColor(Color(255, 255, 255, 100)); // Semi-transparente
        glowEffect.setOrigin(32.0f, 32.0f); // Centraliza o brilho
        glowEffect.setPosition(sprite.getPosition().x + 25, sprite.getPosition().y + 25);

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
        if (lifetime <= 0 || sprite.getPosition().y > 600) {
            active = false;
            return true; // Indica que o power-up deve ser removido
        }
        return false;
    }
};

class Game {
private:
    RenderWindow window;
    vector<Texture> wasteTextures;
    vector<Waste> activeWastes;
    vector<Sprite> bins;
    vector<Text> binLabels;
    map<WasteType, FloatRect> binBounds;
    map<WasteType, Texture> binTextures; // Armazena texturas permanentemente

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

    // --- Adicione estas variáveis na sua classe Game ---
    Texture bgCommunity, bgIndustrial, bgMegacenter, bgBoss;
    Sprite bgSprite;
    bool inLevelTransition = false;
    RectangleShape continueButton;
    Text continueButtonText;
    Text levelInfoText;

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
    float powerUpTextY = 250.0f;

    // --- Novas variáveis para a fase do boss ---
    bool inBossFight = false;
    int playerLife = 100;
    int bossLife = 100;
    RectangleShape playerLifeBar;
    RectangleShape bossLifeBar;
    int correctHitsSinceLastBossPowerUp = 0;

public:
    // --- No construtor ---
    Game() : window(VideoMode(800, 600), "Gerenciador de Reciclagem"), 
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
        reputationBarBack.setSize(Vector2f(200, 20));
        reputationBarBack.setFillColor(Color(50, 50, 50));
        reputationBarBack.setPosition(580, 50);
        
        reputationBar.setSize(Vector2f(200, 20));
        reputationBar.setFillColor(Color(0, 200, 0));
        reputationBar.setPosition(580, 50);

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
        }

        // Carregar texturas dos ícones de som
        if (!soundOnTex.loadFromFile("assets/textures/sound_on.png")) {
            cerr << "Erro ao carregar sound_on.png" << endl;
        }
        if (!soundOffTex.loadFromFile("assets/textures/sound_off.png")) {
            cerr << "Erro ao carregar sound_off.png" << endl;
        }
        soundIcon.setTexture(soundOnTex);
        soundIcon.setScale(0.08f, 0.08f);
        soundIcon.setPosition(730, 20);

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
        bgSprite.setTexture(bgCommunity); // Começa na fase 1

        // Botão de continuar
        continueButton.setSize(Vector2f(220, 60));
        continueButton.setFillColor(Color(70, 130, 180));
        continueButton.setPosition(290, 350);

        continueButtonText.setFont(font);
        continueButtonText.setString("Continuar");
        continueButtonText.setCharacterSize(28);
        continueButtonText.setFillColor(Color::White);
        FloatRect btnTextBounds = continueButtonText.getLocalBounds();
        continueButtonText.setPosition(400 - btnTextBounds.width / 2, 365);

        levelInfoText.setFont(font);
        levelInfoText.setCharacterSize(32);
        levelInfoText.setFillColor(Color::White);
        levelInfoText.setStyle(Text::Bold);
        levelInfoText.setPosition(120, 180);

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
        playerLifeBar.setSize(Vector2f(800, 20));
        playerLifeBar.setFillColor(Color::Green);
        playerLifeBar.setPosition(0, 580);

        bossLifeBar.setSize(Vector2f(800, 20));
        bossLifeBar.setFillColor(Color::Red);
        bossLifeBar.setPosition(0, 0);
    }

    void setupTexts() {
        scoreText.setFont(font);
        scoreText.setCharacterSize(24);
        scoreText.setFillColor(Color::White);
        scoreText.setPosition(20, 20);

        phaseText.setFont(font);
        phaseText.setCharacterSize(24);
        phaseText.setFillColor(Color::White);
        phaseText.setPosition(20, 60);

        reputationText.setFont(font);
        reputationText.setCharacterSize(20);
        reputationText.setFillColor(Color::White);
        reputationText.setPosition(580, 20);

        messageText.setFont(font);
        messageText.setCharacterSize(30);
        messageText.setFillColor(Color::Red);
        messageText.setPosition(200, 250);
        messageText.setStyle(Text::Bold);

        comboText.setFont(font);
        comboText.setCharacterSize(24);
        comboText.setFillColor(Color::White);
        comboText.setPosition(20, 100);

        comboMultiplierText.setFont(font);
        comboMultiplierText.setCharacterSize(24);
        comboMultiplierText.setFillColor(Color::Yellow);
        comboMultiplierText.setStyle(Text::Bold);
        comboMultiplierText.setPosition(150, 100);

        // Configurações para a tela inicial
        gameTitle.setFont(font);
        gameTitle.setString("Gerenciador de Reciclagem");
        gameTitle.setCharacterSize(40);
        gameTitle.setFillColor(Color::White);
        FloatRect titleBounds = gameTitle.getLocalBounds();
        gameTitle.setPosition(400 - titleBounds.width / 2, 120);

        startButton.setSize(Vector2f(220, 60));
        startButton.setFillColor(Color(70, 130, 180));
        startButton.setPosition(290, 300);

        startButtonText.setFont(font);
        startButtonText.setString("Comecar");
        startButtonText.setCharacterSize(28);
        startButtonText.setFillColor(Color::White);
        FloatRect btnTextBounds = startButtonText.getLocalBounds();
        startButtonText.setPosition(400 - btnTextBounds.width / 2, 315);
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

        float binWidth = 70.0f;
        float spacing = (800.0f - (binWidth * binTypes.size())) / (binTypes.size() + 1);

        for (int i = 0; i < binTypes.size(); i++) {
            WasteType type = binTypes[i];

            if (binTextures.find(type) == binTextures.end()) {
                cerr << "Textura não encontrada para lixeira do tipo " << type << endl;
                continue;
            }

            Sprite bin;
            bin.setTexture(binTextures[type]);
            bin.setScale(0.2f, 0.2f);
            float x = spacing + i * (binWidth + spacing);
            float y = 500;
            bin.setPosition(x, y);
            bins.push_back(bin);
            binBounds[type] = bin.getGlobalBounds();

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
            text.setCharacterSize(14);
            text.setFillColor(Color::White);
            FloatRect bounds = bin.getGlobalBounds();
            text.setPosition(x + (binWidth * 0.1f), y + 70); // Ajuste fino se necessário
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
            if (inBossFight) {
                // Na fase do boss, lixo perdido causa dano ao jogador
                playerLife = max(0, playerLife - wastesPassed * 10);
                if (playerLife <= 0) {
                    inDefeatScreen = true;
                    inBossFight = false;
                    messageText.setString("Você perdeu para o Boss!");
                }
            } else if (shieldCount > 0) {
                shieldCount--;
                messageText.setString("Escudo absorveu o erro! (" + to_string(shieldCount) + " restantes)");
                messageText.setFillColor(Color::Blue);
            } else {
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
        
        reputationBar.setSize(Vector2f(2.0f * reputation, 20));
        
        // Atualizar combo text
        ss.str("");
        ss << "Combo: " << combo;
        comboText.setString(ss.str());
        
        ss.str("");
        ss << "x" << fixed << setprecision(1) << comboBoostMultiplier;
        comboMultiplierText.setString(ss.str());
        comboMultiplierText.setPosition(150 + comboText.getLocalBounds().width, 100);
        
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
                messageText.setPosition(400 - messageText.getLocalBounds().width / 2, powerUpTextY);
            }
            if (powerUpTextTimer > 1.5f) {
                messageText.setString(""); // Limpa após 1.5 segundos
                powerUpTextTimer = 0.0f;
                powerUpTextAlpha = 255.0f;
                powerUpTextY = 250.0f;
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

    void handleClick(Vector2f mousePos) {
        // Se nenhum lixo está selecionado, procura por um para selecionar
        if (selectedWasteIndex == -1) {
            for (size_t i = 0; i < activeWastes.size(); ++i) {
                if (activeWastes[i].sprite.getGlobalBounds().contains(mousePos)) {
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
                if (activeWastes[i].sprite.getGlobalBounds().contains(mousePos)) {
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
                    if (bin.second.contains(mousePos)) {
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

    void handlePowerUpClick(Vector2f mousePos) {
        for (size_t i = 0; i < activePowerUps.size(); ++i) {
            // Verifica se o clique foi no efeito de brilho (maior e mais fácil de clicar)
            if (activePowerUps[i].glowEffect.getGlobalBounds().contains(mousePos)) {
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
        IntRect texRect = bgSprite.getTextureRect();
        bgSprite.setScale(
            800.f / texRect.width,
            600.f / texRect.height
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
        RectangleShape overlay(Vector2f(800, 600));
        overlay.setFillColor(Color(0, 0, 0, 200));
        window.draw(bgSprite);
        window.draw(overlay);

        Text defeatText;
        defeatText.setFont(font);
        defeatText.setString("Derrota!\nSua reputacao chegou a zero.\nClique para voltar ao menu.");
        defeatText.setCharacterSize(32);
        defeatText.setFillColor(Color::Red);
        defeatText.setStyle(Text::Bold);
        FloatRect bounds = defeatText.getLocalBounds();
        defeatText.setPosition(400 - bounds.width / 2, 220);
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
        messageText.setPosition(400 - messageText.getLocalBounds().width / 2, powerUpTextY);
        powerUpTextTimer = 0.0f;
        powerUpTextAlpha = 255.0f;
        powerUpTextY = 250.0f;
        comboClock.restart();
    }

    void renderActivePowerUpEffects() {
        float x = 650;
        float y = 100;
        float spacing = 50;

        // Time Freeze
        if (timeFreezeDuration > 0) {
            Sprite icon;
            icon.setTexture(powerUpTextures[PowerUp::TIME_FREEZE]);
            icon.setScale(0.06f, 0.06f);
            icon.setPosition(x, y);
            window.draw(icon);

            // Barra de tempo
            RectangleShape barBack(Vector2f(40, 5));
            barBack.setFillColor(Color(50,50,50));
            barBack.setPosition(x + 50, y + 25);
            window.draw(barBack);

            float ratio = timeFreezeDuration / 5.0f;
            RectangleShape bar(Vector2f(40 * ratio, 5));
            bar.setFillColor(Color::Cyan);
            bar.setPosition(x + 50, y + 25);
            window.draw(bar);
            
            y += spacing;
        }

        // Combo Boost
        if (comboBoostDuration > 0) {
            Sprite icon;
            icon.setTexture(powerUpTextures[PowerUp::COMBO_BOOST]);
            icon.setScale(0.06f, 0.06f);
            icon.setPosition(x, y);
            window.draw(icon);

            // Barra de tempo
            RectangleShape barBack(Vector2f(40, 5));
            barBack.setFillColor(Color(50,50,50));
            barBack.setPosition(x + 50, y + 25);
            window.draw(barBack);

            float ratio = comboBoostDuration / 10.0f;
            RectangleShape bar(Vector2f(40 * ratio, 5));
            bar.setFillColor(Color::Yellow);
            bar.setPosition(x + 50, y + 25);
            window.draw(bar);
            
            // Multiplicador
            Text multText;
            multText.setFont(font);
            multText.setString("x" + to_string(static_cast<int>(comboBoostMultiplier)));
            multText.setCharacterSize(20);
            multText.setFillColor(Color::Yellow);
            multText.setPosition(x + 100, y);
            window.draw(multText);
            
            y += spacing;
        }

        // Magnet
        if (magnetActive) {
            Sprite icon;
            icon.setTexture(powerUpTextures[PowerUp::MAGNET]);
            icon.setScale(0.06f, 0.06f);
            icon.setPosition(x, y);
            window.draw(icon);

            // Barra de tempo
            RectangleShape barBack(Vector2f(40, 5));
            barBack.setFillColor(Color(50,50,50));
            barBack.setPosition(x + 50, y + 25);
            window.draw(barBack);

            float ratio = magnetDuration / 5.0f;
            RectangleShape bar(Vector2f(40 * ratio, 5));
            bar.setFillColor(Color::Green);
            bar.setPosition(x + 50, y + 25);
            window.draw(bar);
            
            y += spacing;
        }

        // Shield
        if (shieldCount > 0) {
            Sprite icon;
            icon.setTexture(powerUpTextures[PowerUp::SHIELD]);
            icon.setScale(0.06f, 0.06f);
            icon.setPosition(x, y);
            window.draw(icon);

            // Contador
            Text countText;
            countText.setFont(font);
            countText.setString(to_string(shieldCount));
            countText.setCharacterSize(24);
            countText.setFillColor(Color::Blue);
            countText.setStyle(Text::Bold);
            countText.setPosition(x + 40, y - 5);
            window.draw(countText);
        }
    }

    void renderLifeBars() {
        if (inBossFight) {
            // Atualiza tamanho das barras
            playerLifeBar.setSize(Vector2f(800.0f * playerLife / 100.0f, 20));
            bossLifeBar.setSize(Vector2f(800.0f * bossLife / 100.0f, 20));
            window.draw(bossLifeBar);
            window.draw(playerLifeBar);
        }
    }

    void run() {
        while (window.isOpen()) {
            Event event;
            while (window.pollEvent(event)) {
                if (event.type == Event::Closed) {
                    window.close();
                }
                if (inStartScreen && event.type == Event::MouseButtonPressed) {
                    if (event.mouseButton.button == Mouse::Left) {
                        Vector2f mousePos(event.mouseButton.x, event.mouseButton.y);
                        if (soundIcon.getGlobalBounds().contains(mousePos)) {
                            soundMuted = !soundMuted;
                            if (soundMuted) {
                                bgMusic.setVolume(0);
                                sound.setVolume(0);
                                victorySound.setVolume(0);
                                defeatSound.setVolume(0);
                                powerUpSound.setVolume(0);
                                soundIcon.setTexture(soundOffTex);
                            } else {
                                bgMusic.setVolume(100);
                                sound.setVolume(100);
                                victorySound.setVolume(100);
                                defeatSound.setVolume(100);
                                powerUpSound.setVolume(100);
                                soundIcon.setTexture(soundOnTex);
                            }
                        }
                        if (startButton.getGlobalBounds().contains(mousePos)) {
                            inStartScreen = false;
                            resetGame();
                            bgMusic.play();
                        }
                    }
                }
                // --- Evento para continuar na tela de transição ---
                if (inLevelTransition && event.type == Event::MouseButtonPressed) {
                    if (event.mouseButton.button == Mouse::Left) {
                        Vector2f mousePos(event.mouseButton.x, event.mouseButton.y);
                        if (continueButton.getGlobalBounds().contains(mousePos)) {
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
                                inBossFight = true;
                                activeWastes.clear();
                                activePowerUps.clear();
                                combo = 0;
                                playerLife = 100;
                                bossLife = 100;
                                correctHitsSinceLastBossPowerUp = 0;
                                bgMusic.play();
                            } else if (phase == BOSS) {
                                inStartScreen = true;
                                resetGame();
                                bgMusic.play();
                            }
                        }
                    }
                }
                // --- Evento para derrota: clique para voltar ao menu ---
                if (inDefeatScreen && event.type == Event::MouseButtonPressed) {
                    if (event.mouseButton.button == Mouse::Left) {
                        inDefeatScreen = false;
                        inStartScreen = true;
                        resetGame();
                        bgMusic.play();
                    }
                }
                if (!inStartScreen && !inLevelTransition && !inDefeatScreen && event.type == Event::MouseButtonPressed) {
                    if (event.mouseButton.button == Mouse::Left) {
                        Vector2f mousePos(event.mouseButton.x, event.mouseButton.y);

                        // Primeiro verifica power-ups
                        handlePowerUpClick(mousePos);

                        // Depois verifica lixos e lixeiras
                        handleClick(mousePos);
                    }
                }
            }

            window.clear(Color(30, 70, 40));

            if (inStartScreen) {
                bgSprite.setTexture(bgCommunity, true);
                IntRect texRect = bgSprite.getTextureRect();
                bgSprite.setScale(
                    800.f / texRect.width,
                    600.f / texRect.height
                );
                window.draw(bgSprite);

                RectangleShape overlay(Vector2f(800, 600));
                overlay.setFillColor(Color(0, 0, 0, 100));
                window.draw(overlay);

                window.draw(gameTitle);
                window.draw(startButton);
                window.draw(startButtonText);
                window.draw(soundIcon);
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
            
            window.draw(scoreText);
            window.draw(phaseText);
            
            // Não mostrar reputação na fase do boss
            if (!inBossFight) {
                window.draw(reputationText);
                window.draw(reputationBarBack);
                window.draw(reputationBar);
            }
            
            window.draw(comboText);
            
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