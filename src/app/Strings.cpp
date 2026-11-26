#include "Strings.hpp"

#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace blitzdj::app
{
    namespace
    {
        bool portuguese = false;

        constexpr const char* english[] = {
            "Your intelligent party DJ, powered by Spotify.",
            "Connect with Spotify",
            "Finish the login in your browser...",
            "Cancel",
            "Spotify login failed",
            "Player",
            "AutoDJ",
            "Search",
            "Speakers",
            "NOW PLAYING",
            "Playing on %s",
            "Nothing playing",
            "Start Spotify on any device",
            "Up next",
            "Off — click to set it up",
            "Queueing %s",
            "Enable AutoDJ",
            "BlitzDJ picks and queues new tracks before the current one ends, avoiding repeats and mixing artists.",
            "Genre, mood or artist",
            "Queue 5 picks now",
            "Suggestions",
            "Click refresh to preview what AutoDJ would play.",
            "Refresh",
            "Songs, artists or albums",
            "Search Spotify and play or queue any track.",
            "No results.",
            "Added to the queue.",
            "Done.",
            "%s added to favorites.",
            "Audio output",
            "Party mode",
            "Keeps AutoDJ running and reconnects your Bluetooth speaker if it drops, resuming the music.",
            "Speaker reconnected — music resumed.",
            "Bluetooth speakers",
            "No Bluetooth speakers found.",
            "Bluetooth control is available on Linux. Use the system settings to pick a speaker.",
            "Bluetooth settings",
            "Connected",
            "Paired",
            "Connect",
            "Disconnect",
            "Spotify Connect",
            "No Spotify devices found. Open Spotify on a phone, computer or speaker.",
            "Play here",
            "Active",
            "Open Spotify",
            "Log out",
            "Volume",
            "Scan",
            "Speaker reconnected."
        };

        constexpr const char* portugueseTexts[] = {
            "Seu DJ inteligente para festas, com Spotify.",
            "Conectar com Spotify",
            "Termine o login no navegador...",
            "Cancelar",
            "Falha no login do Spotify",
            "Player",
            "AutoDJ",
            "Buscar",
            "Caixas",
            "TOCANDO AGORA",
            "Tocando em %s",
            "Nada tocando",
            "Abra o Spotify em algum dispositivo",
            "A seguir",
            "Desligado — clique para configurar",
            "Tocando %s",
            "Ativar AutoDJ",
            "O BlitzDJ escolhe e enfileira músicas novas antes da atual acabar, evitando repetições e variando os artistas.",
            "Gênero, clima ou artista",
            "Enfileirar 5 agora",
            "Sugestões",
            "Clique em atualizar para ver o que o AutoDJ tocaria.",
            "Atualizar",
            "Músicas, artistas ou álbuns",
            "Busque no Spotify e toque ou enfileire qualquer música.",
            "Nenhum resultado.",
            "Adicionada à fila.",
            "Pronto.",
            "%s adicionado aos favoritos.",
            "Saída de áudio",
            "Modo festa",
            "Mantém o AutoDJ rodando e reconecta a caixa Bluetooth se ela cair, voltando a tocar.",
            "Caixa reconectada — música retomada.",
            "Caixas Bluetooth",
            "Nenhuma caixa Bluetooth encontrada.",
            "O controle de Bluetooth funciona no Linux. Use as configurações do sistema para escolher a caixa.",
            "Configurações de Bluetooth",
            "Conectada",
            "Pareada",
            "Conectar",
            "Desconectar",
            "Spotify Connect",
            "Nenhum dispositivo do Spotify encontrado. Abra o Spotify em um celular, computador ou caixa.",
            "Tocar aqui",
            "Ativo",
            "Abrir Spotify",
            "Sair da conta",
            "Volume",
            "Procurar",
            "Caixa reconectada."
        };

        static_assert(sizeof(english) / sizeof(english[0]) == static_cast<int>(Text::Count));
        static_assert(sizeof(portugueseTexts) / sizeof(portugueseTexts[0]) == static_cast<int>(Text::Count));

        bool startsWithPortuguese(const char* value)
        {
            return value && std::strncmp(value, "pt", 2) == 0;
        }
    }

    void initializeLanguage()
    {
#ifdef _WIN32
        portuguese = PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_PORTUGUESE;
#else
        portuguese = startsWithPortuguese(std::getenv("LC_ALL")) ||
            startsWithPortuguese(std::getenv("LC_MESSAGES")) ||
            startsWithPortuguese(std::getenv("LANG")) ||
            startsWithPortuguese(std::getenv("LANGUAGE"));
#endif
    }

    const char* tr(Text text)
    {
        const auto index = static_cast<int>(text);
        return portuguese ? portugueseTexts[index] : english[index];
    }
}
