/**
 * @file Console.hpp
 * @brief L'entree terminal de la borne, lue SANS bloquer.
 *
 * Une borne qui n'a plus de fenetre n'a plus de clavier : plus moyen de
 * choisir une bibliotheque, donc plus moyen d'en retrouver une. Le terminal
 * est la seule entree qui ne depend d'aucun vendor.
 *
 * TOUT L'ENJEU EST DE NE PAS BLOQUER. Un scanf ou un getline arreterait la
 * boucle : les jeux cesseraient d'avancer, et la borne ne repondrait plus
 * tant que personne n'a tape. On interroge donc l'entree avec un delai nul
 * et on ne lit que ce qui est deja arrive.
 *
 * Le terminal reste en mode ligne - pas de mode brut, pas de termios a
 * restaurer. L'utilisateur tape et valide, comme dans un shell, et ca
 * fonctionne aussi quand l'entree est un tube ou un fichier de test.
 */

#ifndef CONSOLE_HPP_
#define CONSOLE_HPP_

#include <poll.h>
#include <unistd.h>

#include <optional>
#include <string>

class Console {

    public:
        /**
         * @brief La prochaine ligne complete, si elle est deja arrivee.
         *
         * @return std::nullopt quand rien n'attend - le cas de presque
         *         toutes les frames.
         */
        std::optional<std::string> poll() {
            drain();

            const size_t end = _buffer.find('\n');

            if (end == std::string::npos)
                return std::nullopt;

            std::string line = _buffer.substr(0, end);

            _buffer.erase(0, end + 1);
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            return line;
        }

        /** @brief L'entree est-elle definitivement close (Ctrl-D, tube vide) ? */
        bool closed() const { return _closed; }

    private:
        /** @brief Aspire ce qui est disponible, et rien de plus. */
        void drain() {
            char chunk[512];

            while (ready()) {
                const ssize_t got = ::read(STDIN_FILENO, chunk, sizeof(chunk));

                if (got > 0) {
                    _buffer.append(chunk, static_cast<size_t>(got));
                    continue;
                }
                /* 0 = fin de fichier. Negatif = rien a lire pour l'instant,
                 * ce qui n'est pas une erreur : on repassera. */
                if (got == 0)
                    _closed = true;
                return;
            }
        }

        /** @brief Y a-t-il quelque chose a lire, la, tout de suite ? */
        static bool ready() {
            struct pollfd watched{STDIN_FILENO, POLLIN, 0};

            //delai nul : on demande l'etat, on n'attend pas qu'il change
            return ::poll(&watched, 1, 0) > 0 && (watched.revents & POLLIN);
        }

        std::string _buffer;
        bool _closed = false;
};

#endif /* !CONSOLE_HPP_ */
