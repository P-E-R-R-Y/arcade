/**
 * @file Games.hpp
 * @brief Les jeux en cours. Plusieurs a la fois.
 */

#ifndef GAMES_HPP_
#define GAMES_HPP_

#include "IAppModule.hpp"
#include "ICore.hpp"
#include "IModuleManager.hpp"

#include <string>
#include <vector>

/**
 * @class Games
 * @brief Les jeux lances, et rien d'autre : les prendre, les ticker, les rendre.
 *
 * Plusieurs, et c'est tout l'interet d'ITickable public : la borne les fait
 * avancer un par un dans sa propre boucle, chacun avec sa fenetre. Ce qui
 * les limite n'est pas la borne mais le materiel - deux jeux ne peuvent pas
 * tenir le meme vendor OpenGL.
 */
class Games {

    public:
        explicit Games(IModuleManager &modules) : _modules(modules) {}
        ~Games() { quit(); }

        Games(const Games &) = delete;
        Games &operator=(const Games &) = delete;

        /**
         * @brief Lance un jeu. Il se debrouille pour trouver de quoi dessiner.
         *
         * On ne lui passe ni fenetre ni module graphique : il recoit le
         * manager et va chercher lui-meme.
         *
         * @return "" si tout va bien, sinon de quoi remplir le bandeau
         */
        std::string play(IAppModule *module) {
            if (running(module))
                return "";

            /* Detenteur avant tout : l'application est allouee par cette dll
             * et sa vtable y vit. Sans ce compteur, Reconcile() fermerait la
             * bibliotheque sous une application vivante. */
            module->acquire();

            IApp *app = module->createApp(_modules);

            if (!app) {
                module->release();
                return std::string(module->name()) + " : createApp a echoue";
            }
            _running.push_back({module, app});
            return "";
        }

        /** @brief Ce jeu tourne-t-il deja ? */
        bool running(IAppModule *module) const {
            for (const Running &game : _running)
                if (game.module == module)
                    return true;
            return false;
        }

        /** @brief Y a-t-il au moins un jeu ? */
        bool empty() const { return _running.empty(); }

        /** @brief Arrete tous les jeux. */
        void quit() {
            while (!_running.empty())
                quit(_running.back().module);
        }

        /** @brief Arrete CE jeu. La dll qui a alloue libere. */
        void quit(IAppModule *module) {
            for (size_t i = 0; i < _running.size(); i++) {
                if (_running[i].module != module)
                    continue;

                /* L'ordre, comme partout : detruire pendant que la dll vit,
                 * et relacher seulement apres. */
                module->deleteApp(_running[i].app);
                module->release();
                _running.erase(_running.begin() + i);
                return;
            }
        }

        void event()   { for (const Running &game : _running) game.app->event(); }
        void update()  { for (const Running &game : _running) game.app->update(); }
        void display() { for (const Running &game : _running) game.app->display(); }

        /**
         * @brief Arrete ceux dont la bibliotheque est condamnee.
         *
         * C'est la borne qui les tient, donc c'est a elle de lacher : sans
         * ca, Reconcile() ne fermerait jamais leur colonne.
         *
         * @return le nom du dernier arrete, "" si aucun
         */
        std::string dropCondemned() {
            std::string stopped;

            for (size_t i = _running.size(); i-- > 0; )
                if (_running[i].module->mustClose()) {
                    stopped = _running[i].module->name();
                    quit(_running[i].module);
                }
            return stopped;
        }

        /**
         * @brief Arrete ceux qui se sont arretes eux-memes.
         *
         * Un jeu qui appelle stop() rend la main a la borne, il ne la ferme
         * pas.
         */
        void dropStopped() {
            for (size_t i = _running.size(); i-- > 0; )
                if (!_running[i].app->running())
                    quit(_running[i].module);
        }

        /** @brief Les noms des jeux en cours, separes par des virgules. */
        std::string names() const {
            std::string all;

            for (const Running &game : _running)
                all += (all.empty() ? "" : ", ") + std::string(game.module->name());
            return all;
        }

        /** @brief Le premier lance, pour un message. nullptr si aucun. */
        IAppModule *first() const {
            return _running.empty() ? nullptr : _running.front().module;
        }

    private:
        /** @brief Un jeu en cours : sa fabrique, et ce qu'elle a construit. */
        struct Running {
            IAppModule *module;   ///< la fabrique, tenue tant qu'il tourne
            IApp *app;            ///< le jeu, detruit par cette meme fabrique
        };

        IModuleManager &_modules;
        std::vector<Running> _running;
};

#endif /* !GAMES_HPP_ */
