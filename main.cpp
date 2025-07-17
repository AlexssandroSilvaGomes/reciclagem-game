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
    MEGACENTER
};

class Waste {
public:
    WasteType type;
    Sprite sprite;
    Vector2f velocity;
    bool active;

    Waste(Texture& texture, WasteType t) : type(t), active(true) {
        sprite.setTexture(texture);
        sprite.setScale(0.12f, 0.12f);
        sprite.setPosition(rand() % 600, -50);
        velocity = Vector2f(0, rand() % 3 + 1);
        sprite.setColor(Color::White);
    }

    void update() {
        sprite.move(velocity);
        if (sprite.getPosition().y > 600) active = false;
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

    Waste* selectedWaste;

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
    Texture bgCommunity, bgIndustrial, bgMegacenter;
    Sprite bgSprite;
    bool inLevelTransition = false;
    RectangleShape continueButton;
    Text continueButtonText;
    Text levelInfoText;

    SoundBuffer victoryBuffer, defeatBuffer;
    Sound victorySound, defeatSound;
    bool inDefeatScreen = false;

public:
    Game() : window(VideoMode(800, 600), "Gerenciador de Reciclagem"), 
              score(0), reputation(100), phase(0), combo(0), spawnTimer(0), 
              gameTimer(0), eventTimer(0), specialEvent(false), selectedWaste(nullptr), inStartScreen(true) {
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

        // --- No construtor, carregue os backgrounds e configure o botão de continuar ---
        if (!bgCommunity.loadFromFile("assets/textures/bg_community.png")) {
            cerr << "Erro ao carregar bg_community.png" << endl;
        }
        if (!bgIndustrial.loadFromFile("assets/textures/bg_industrial.png")) {
            cerr << "Erro ao carregar bg_industrial.png" << endl;
        }
        if (!bgMegacenter.loadFromFile("assets/textures/bg_megacenter.png")) {
            cerr << "Erro ao carregar bg_megacenter.png" << endl;
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

        // --- No construtor, carregue os sons de vitória e derrota ---
        if (!victoryBuffer.loadFromFile("assets/sounds/victory.mp3")) {
            cerr << "Erro ao carregar victory.wav" << endl;
        }
        victorySound.setBuffer(victoryBuffer);

        if (!defeatBuffer.loadFromFile("assets/sounds/defeat.wav")) {
            cerr << "Erro ao carregar defeat.wav" << endl;
        }
        defeatSound.setBuffer(defeatBuffer);
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
        } else {
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

        activeWastes.push_back(Waste(wasteTextures[type], type));
    }

    void update() {
        float deltaTime = 1.0f / 60.0f;
        spawnTimer += deltaTime;
        gameTimer += deltaTime;

        // Atualizar resíduos
        for (auto& waste : activeWastes) {
            waste.update();
        }

        // Remover resíduos inativos
        activeWastes.erase(remove_if(activeWastes.begin(), activeWastes.end(), 
            [&](const Waste& w) {
                if (!w.active) {
                    if (selectedWaste == &w) {
                        selectedWaste = nullptr;
                    }
                    return true;
                }
                return false;
            }), activeWastes.end());

        // Gerar novos resíduos
        float spawnInterval = 2.0f - phase * 0.5f;
        if (spawnTimer > spawnInterval && activeWastes.size() < 5 + phase * 2) {
            spawnWaste();
            spawnTimer = 0;
        }

        // Eventos especiais na fase 3
        if (phase == MEGACENTER) {
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
        else ss << "Megacentro Urbano";
        phaseText.setString(ss.str());
        
        ss.str("");
        ss << "Reputacao: " << reputation << "%";
        reputationText.setString(ss.str());
        
        reputationBar.setSize(Vector2f(2.0f * reputation, 20));
    }

    void handleClick(Vector2f mousePos) {
        if (selectedWaste == nullptr) {
            for (auto it = activeWastes.begin(); it != activeWastes.end(); ++it) {
                if (it->sprite.getGlobalBounds().contains(mousePos)) {
                    selectedWaste = &(*it);
                    it->sprite.setColor(Color(255, 255, 0));
                    sound.setBuffer(selectBuffer);      // <-- toque o som de seleção aqui
                    sound.play();
                    break;
                }
            }
        }
        else {
            bool correct = false;
            WasteType selectedType = selectedWaste->type;

            for (const auto& bin : binBounds) {
                if (bin.second.contains(mousePos)) {
                    if (bin.first == selectedType) {
                        correct = true;
                    }
                    break;
                }
            }

            if (correct) {
                sound.setBuffer(correctBuffer);
                sound.play();
                int points = 5 + (combo * 2);
                score += points;
                combo++;
                comboClock.restart();
                reputation = min(100, reputation + 2);
            } else {
                sound.setBuffer(wrongBuffer);
                sound.play();
                combo = 0;
                reputation = max(0, reputation - 10);
                if (reputation <= 0) {
                    bgMusic.pause();
                    defeatSound.play();
                    inDefeatScreen = true;
                }
            }

            auto it = find_if(activeWastes.begin(), activeWastes.end(),
                [&](const Waste& w) { return &w == selectedWaste; });
            if (it != activeWastes.end()) {
                activeWastes.erase(it);
            }

            selectedWaste = nullptr;
        }
    }

    // --- Adicione uma função para atualizar o background conforme a fase ---
    void updateBackground() {
        if (phase == COMMUNITY)
            bgSprite.setTexture(bgCommunity, true);
        else if (phase == INDUSTRIAL)
            bgSprite.setTexture(bgIndustrial, true);
        else
            bgSprite.setTexture(bgMegacenter, true);

        // Ajusta o tamanho do background para preencher a janela
        IntRect texRect = bgSprite.getTextureRect();
        bgSprite.setScale(
            800.f / texRect.width,
            600.f / texRect.height
        );
    }

    // --- Modifique checkPhaseTransition para mostrar a tela de transição ---
    void checkPhaseTransition() {
        if (phase == COMMUNITY && score >= 20 && !inLevelTransition) {
            inLevelTransition = true;
            levelInfoText.setString("Fase 1 completa!\nPontuacao: " + to_string(score) +
                               "\nReputacao: " + to_string(reputation) + "%");
            bgMusic.pause();
            victorySound.play();
        } else if (phase == INDUSTRIAL && score >= 40 && !inLevelTransition) {
            inLevelTransition = true;
            levelInfoText.setString("Fase 2 completa!\nPontuacao: " + to_string(score) +
                               "\nReputacao: " + to_string(reputation) + "%");
            bgMusic.pause();
            victorySound.play();
        } else if (phase == MEGACENTER && score >= 60 && !inLevelTransition) {
            inLevelTransition = true;
            levelInfoText.setString("Parabens! Jogo completo!\nPontuacao final: " + to_string(score) +
                               "\nReputacao final: " + to_string(reputation) + "%");
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
        specialEvent = false;
        selectedWaste = nullptr;
        activeWastes.clear();
        inLevelTransition = false;
        inDefeatScreen = false;
        setupBins();
        updateBackground();
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
                                soundIcon.setTexture(soundOffTex);
                            } else {
                                bgMusic.setVolume(100);
                                sound.setVolume(100);
                                victorySound.setVolume(100);
                                defeatSound.setVolume(100);
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
                                // Volta ao menu inicial após última fase
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
                        handleClick(Vector2f(event.mouseButton.x, event.mouseButton.y));
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
            window.draw(scoreText);
            window.draw(phaseText);
            window.draw(reputationText);
            window.draw(reputationBarBack);
            window.draw(reputationBar);

            if (specialEvent) {
                window.draw(messageText);
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